#include "nix/cmd/command-installable-value.hh"
#include "nix/store/globals.hh"
#include "nix/expr/eval.hh"
#include "nix/expr/eval-inline.hh"
#include "nix/expr/eval-settings.hh"
#include "nix/store/names.hh"
#include "nix/expr/get-drvs.hh"
#include "nix/main/common-args.hh"
#include "nix/main/shared.hh"
#include "nix/expr/eval-cache.hh"
#include "nix/expr/attr-path.hh"
#include "nix/expr/search.hh"
#include "nix/util/hilite.hh"
#include "nix/util/strings-inline.hh"

#include <regex>
#include <fstream>
#include <nlohmann/json.hpp>

#include "nix/util/strings.hh"

using namespace nix;
using json = nlohmann::json;

std::string wrap(std::string prefix, std::string s)
{
    return concatStrings(prefix, s, ANSI_NORMAL);
}

struct CmdSearch : InstallableValueCommand, MixJSON
{
    std::vector<std::string> res;
    std::vector<std::string> excludeRes;

    CmdSearch()
    {
        expectArgs("regex", &res);
        addFlag(
            Flag{
                .longName = "exclude",
                .shortName = 'e',
                .description = "Hide packages whose attribute path, name or description contain *regex*.",
                .labels = {"regex"},
                .handler = {[this](std::string s) { excludeRes.push_back(s); }},
            });
    }

    std::string description() override
    {
        return "search for packages";
    }

    std::string doc() override
    {
        return
#include "search.md"
            ;
    }

    Strings getDefaultFlakeAttrPaths() override
    {
        return {"packages." + settings.thisSystem.get(), "legacyPackages." + settings.thisSystem.get()};
    }

    void run(ref<Store> store, ref<InstallableValue> installable) override
    {
        settings.readOnlyMode = true;
        evalSettings.enableImportFromDerivation.setDefault(false);

        // Recommend "^" here instead of ".*" due to differences in resulting highlighting
        if (res.empty())
            throw UsageError(
                "Must provide at least one regex! To match all packages, use '%s'.", "nix search <installable> ^");

        std::vector<std::regex> regexes;
        std::vector<std::regex> excludeRegexes;
        regexes.reserve(res.size());
        excludeRegexes.reserve(excludeRes.size());

        for (auto & re : res)
            regexes.push_back(std::regex(re, std::regex::extended | std::regex::icase));

        for (auto & re : excludeRes)
            excludeRegexes.emplace_back(re, std::regex::extended | std::regex::icase);

        auto state = getEvalState();

        std::optional<nlohmann::json> jsonOut;
        if (json)
            jsonOut = json::object();

        uint64_t results = 0;

        for (auto & cursor : installable->getCursors(*state)) {
            // Use depth=3 for flake compatibility: installable -> legacyPackages/packages -> system -> packages
            searchDerivations(
                *state, *cursor, regexes, excludeRegexes,
                [&](const SearchMatch & match) {
                    results++;
                    if (json) {
                        (*jsonOut)[match.attrPath] = {
                            {"pname", match.pname},
                            {"version", match.version},
                            {"description", match.description},
                        };
                    } else {
                        // Re-run regexes to get match positions for highlighting
                        std::vector<std::smatch> attrPathMatches;
                        std::vector<std::smatch> descriptionMatches;
                        for (auto & regex : regexes) {
                            auto addAll = [](std::sregex_iterator it, std::vector<std::smatch> & vec) {
                                const auto end = std::sregex_iterator();
                                while (it != end) {
                                    vec.push_back(*it++);
                                }
                            };
                            addAll(
                                std::sregex_iterator(match.attrPath.begin(), match.attrPath.end(), regex),
                                attrPathMatches);
                            addAll(
                                std::sregex_iterator(match.description.begin(), match.description.end(), regex),
                                descriptionMatches);
                        }

                        if (results > 1)
                            logger->cout("");
                        logger->cout(
                            "* %s%s",
                            wrap("\e[0;1m", hiliteMatches(match.attrPath, attrPathMatches, ANSI_GREEN, "\e[0;1m")),
                            match.version != "" ? " (" + match.version + ")" : "");
                        if (match.description != "")
                            logger->cout(
                                "  %s",
                                hiliteMatches(match.description, descriptionMatches, ANSI_GREEN, ANSI_NORMAL));
                    }
                    return true; // continue searching
                },
                3 // recurseDepth for flake compatibility
            );
        }

        if (json)
            printJSON(*jsonOut);

        if (!json && !results)
            throw Error("no results for the given search term(s)!");
    }
};

static auto rCmdSearch = registerCommand<CmdSearch>("search");
