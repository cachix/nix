#include "nix/store/build-environment.hh"
#include "nix/store/derived-path.hh"
#include "nix/store/globals.hh"
#include "nix/util/file-system.hh"
#include "nix/util/source-accessor.hh"
#include "nix/util/strings.hh"

namespace nix {

// Embedded get-env.sh script for capturing the build environment
static const std::string getEnvSh =
#include "get-env.sh.gen.hh"
    ;

BuildEnvironment BuildEnvironment::fromJSON(const nlohmann::json & json)
{
    BuildEnvironment res;

    StringSet exported;

    for (auto & [name, info] : json["variables"].items()) {
        std::string type = info["type"];
        if (type == "var" || type == "exported")
            res.vars.insert(
                {name, BuildEnvironment::String{.exported = type == "exported", .value = info["value"]}});
        else if (type == "array")
            res.vars.insert({name, (Array) info["value"]});
        else if (type == "associative")
            res.vars.insert({name, (Associative) info["value"]});
    }

    for (auto & [name, def] : json["bashFunctions"].items()) {
        res.bashFunctions.insert({name, def});
    }

    if (json.contains("structuredAttrs")) {
        res.structuredAttrs = {json["structuredAttrs"][".attrs.json"], json["structuredAttrs"][".attrs.sh"]};
    }

    return res;
}

BuildEnvironment BuildEnvironment::parseJSON(std::string_view in)
{
    auto json = nlohmann::json::parse(in);
    return fromJSON(json);
}

BuildEnvironment BuildEnvironment::fromDerivation(const Store & store, const Derivation & drv)
{
    BuildEnvironment res;

    // Copy environment variables from the derivation
    // All derivation env vars are treated as exported since they come from the derivation itself
    for (const auto & [name, value] : drv.env) {
        res.vars.insert({name, BuildEnvironment::String{.exported = true, .value = value}});
    }

    // Extract structured attributes if present
    if (drv.structuredAttrs.has_value()) {
        const auto & structAttrs = drv.structuredAttrs.value().structuredAttrs;

        // Look for .attrs.json and .attrs.sh in the structured attributes
        if (structAttrs.contains(".attrs.json") && structAttrs.contains(".attrs.sh")) {
            // Both should be strings containing the respective content
            const auto & json_val = structAttrs.at(".attrs.json");
            const auto & sh_val = structAttrs.at(".attrs.sh");
            if (json_val.is_string() && sh_val.is_string()) {
                res.structuredAttrs = {
                    json_val.get<std::string>(),
                    sh_val.get<std::string>()
                };
            }
        }
    }

    return res;
}

nlohmann::json BuildEnvironment::toJSON() const
{
    auto res = nlohmann::json::object();

    auto vars2 = nlohmann::json::object();
    for (auto & [name, value] : vars) {
        auto info = nlohmann::json::object();
        if (auto str = std::get_if<String>(&value)) {
            info["type"] = str->exported ? "exported" : "var";
            info["value"] = str->value;
        } else if (auto arr = std::get_if<Array>(&value)) {
            info["type"] = "array";
            info["value"] = *arr;
        } else if (auto arr = std::get_if<Associative>(&value)) {
            info["type"] = "associative";
            info["value"] = *arr;
        }
        vars2[name] = std::move(info);
    }
    res["variables"] = std::move(vars2);

    res["bashFunctions"] = bashFunctions;

    if (providesStructuredAttrs()) {
        auto contents = nlohmann::json::object();
        contents[".attrs.sh"] = getAttrsSH();
        contents[".attrs.json"] = getAttrsJSON();
        res["structuredAttrs"] = std::move(contents);
    }

    assert(BuildEnvironment::fromJSON(res) == *this);

    return res;
}

void BuildEnvironment::toBash(std::ostream & out, const StringSet & ignoreVars) const
{
    for (auto & [name, value] : vars) {
        if (!ignoreVars.count(name)) {
            if (auto str = std::get_if<String>(&value)) {
                out << fmt("%s=%s\n", name, escapeShellArgAlways(str->value));
                if (str->exported)
                    out << fmt("export %s\n", name);
            } else if (auto arr = std::get_if<Array>(&value)) {
                out << "declare -a " << name << "=(";
                for (auto & s : *arr)
                    out << escapeShellArgAlways(s) << " ";
                out << ")\n";
            } else if (auto arr = std::get_if<Associative>(&value)) {
                out << "declare -A " << name << "=(";
                for (auto & [n, v] : *arr)
                    out << "[" << escapeShellArgAlways(n) << "]=" << escapeShellArgAlways(v) << " ";
                out << ")\n";
            }
        }
    }

    for (auto & [name, def] : bashFunctions) {
        out << name << " ()\n{\n" << def << "}\n";
    }
}

std::string BuildEnvironment::getString(const Value & value)
{
    if (auto str = std::get_if<String>(&value))
        return str->value;
    else
        throw Error("bash variable is not a string");
}

BuildEnvironment::Associative BuildEnvironment::getAssociative(const Value & value)
{
    if (auto assoc = std::get_if<Associative>(&value))
        return *assoc;
    else
        throw Error("bash variable is not an associative array");
}

BuildEnvironment::Array BuildEnvironment::getStrings(const Value & value)
{
    if (auto str = std::get_if<String>(&value))
        return tokenizeString<Array>(str->value);
    else if (auto arr = std::get_if<Array>(&value)) {
        return *arr;
    } else if (auto assoc = std::get_if<Associative>(&value)) {
        Array assocKeys;
        std::for_each(assoc->begin(), assoc->end(), [&](auto & n) { assocKeys.push_back(n.first); });
        return assocKeys;
    } else
        throw Error("bash variable is not a string or array");
}

std::string BuildEnvironment::getSystem() const
{
    if (auto v = get(vars, "system"))
        return getString(*v);
    else
        return settings.thisSystem;
}

std::pair<StorePath, BuildEnvironment> BuildEnvironment::getDevEnvironment(ref<Store> store, const StorePath & drvPath)
{
    auto drv = store->derivationFromPath(drvPath);

    auto builder = baseNameOf(drv.builder);
    if (builder != "bash")
        throw Error("'getDevEnvironment' only works on derivations that use 'bash' as their builder");

    // Add get-env.sh to the store
    auto getEnvShPath = ({
        StringSource source{getEnvSh};
        store->addToStoreFromDump(
            source,
            "get-env.sh",
            FileSerialisationMethod::Flat,
            ContentAddressMethod::Raw::Text,
            HashAlgorithm::SHA256,
            {});
    });

    // Create a modified derivation that runs get-env.sh
    drv.args = {store->printStorePath(getEnvShPath)};

    // Remove derivation checks
    drv.env.erase("allowedReferences");
    drv.env.erase("allowedRequisites");
    drv.env.erase("disallowedReferences");
    drv.env.erase("disallowedRequisites");
    drv.env.erase("name");

    // Rehash and write the derivation
    drv.name += "-env";
    drv.env.emplace("name", drv.name);
    drv.inputSrcs.insert(std::move(getEnvShPath));

    if (experimentalFeatureSettings.isEnabled(Xp::CaDerivations)) {
        for (auto & output : drv.outputs) {
            output.second = DerivationOutput::Deferred{};
            drv.env[output.first] = hashPlaceholder(output.first);
        }
    } else {
        for (auto & output : drv.outputs) {
            output.second = DerivationOutput::Deferred{};
            drv.env[output.first] = "";
        }
        auto hashesModulo = hashDerivationModulo(*store, drv, true);

        for (auto & output : drv.outputs) {
            Hash h = hashesModulo.hashes.at(output.first);
            auto outPath = store->makeOutputPath(output.first, h, drv.name);
            output.second = DerivationOutput::InputAddressed{
                .path = outPath,
            };
            drv.env[output.first] = store->printStorePath(outPath);
        }
    }

    auto shellDrvPath = store->writeDerivation(drv);

    // Build the derivation
    store->buildPaths(
        {DerivedPath::Built{
            .drvPath = makeConstantStorePathRef(shellDrvPath),
            .outputs = OutputsSpec::All{},
        }},
        bmNormal,
        store);

    // Find the output with content
    for (auto & [_0, optPath] : store->queryPartialDerivationOutputMap(shellDrvPath)) {
        assert(optPath);
        auto & outPath = *optPath;
        assert(store->isValidPath(outPath));
        auto accessor = store->requireStoreObjectAccessor(outPath);
        if (auto st = accessor->maybeLstat(CanonPath::root); st && st->fileSize.value_or(0)) {
            // Read and parse the JSON output
            auto buildEnv = BuildEnvironment::parseJSON(accessor->readFile(CanonPath::root));

            // Filter out sandbox-specific variables that shouldn't leak into the shell
            // (same as ignoreVars in develop.cc)
            for (const auto & var : StringSet{
                "BASHOPTS",
                "HOME",
                "NIX_BUILD_TOP",
                "NIX_ENFORCE_PURITY",
                "NIX_LOG_FD",
                "NIX_REMOTE",
                "PPID",
                "SHELLOPTS",
                "SSL_CERT_FILE",
                "TEMP",
                "TEMPDIR",
                "TERM",
                "TMP",
                "TMPDIR",
                "TZ",
                "UID",
            }) {
                buildEnv.vars.erase(var);
            }

            return {outPath, std::move(buildEnv)};
        }
    }

    throw Error("get-env.sh failed to produce an environment");
}

} // namespace nix
