#include "nix/store/build-environment.hh"
#include "nix/store/globals.hh"
#include "nix/util/strings.hh"

namespace nix {

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

} // namespace nix
