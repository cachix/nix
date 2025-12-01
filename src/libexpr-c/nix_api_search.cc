#include "nix_api_search.h"
#include "nix_api_attr_cursor.h"
#include "nix_api_attr_cursor_internal.h"
#include "nix_api_util_internal.h"

#include "nix/store/names.hh"

#include <algorithm>
#include <functional>
#include <regex>
#include <string>
#include <vector>

struct nix_search_params {
    std::vector<std::regex> includeRegexes;
    std::vector<std::regex> excludeRegexes;
};

nix_search_params * nix_search_params_new(nix_c_context * context)
{
    if (context) nix_clear_err(context);
    try {
        return new nix_search_params();
    } NIXC_CATCH_ERRS_NULL
}

void nix_search_params_free(nix_search_params * params)
{
    delete params;
}

nix_err nix_search_params_add_regex(
    nix_c_context * context,
    nix_search_params * params,
    const char * pattern)
{
    if (context) nix_clear_err(context);
    try {
        params->includeRegexes.emplace_back(pattern, std::regex::extended | std::regex::icase);
        return NIX_OK;
    } NIXC_CATCH_ERRS
}

nix_err nix_search_params_add_exclude(
    nix_c_context * context,
    nix_search_params * params,
    const char * pattern)
{
    if (context) nix_clear_err(context);
    try {
        params->excludeRegexes.emplace_back(pattern, std::regex::extended | std::regex::icase);
        return NIX_OK;
    } NIXC_CATCH_ERRS
}

nix_err nix_search(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    nix_search_params * params,
    nix_search_callback callback,
    void * user_data)
{
    if (context) nix_clear_err(context);
    try {
        auto & state = cursor->cache->state;

        // Use empty params if none provided
        nix_search_params defaultParams;
        nix_search_params * p = params ? params : &defaultParams;

        // Track whether to continue searching
        bool continueSearch = true;

        // Recursive visit function (mirrors search.cc logic)
        std::function<void(nix::eval_cache::AttrCursor &, const std::vector<nix::Symbol> &, bool)> visit;

        visit = [&](nix::eval_cache::AttrCursor & cur, const std::vector<nix::Symbol> & attrPath, bool initialRecurse) {
            if (!continueSearch) return;

            auto attrPathS = state.symbols.resolve(attrPath);

            try {
                auto recurse = [&]() {
                    for (const auto & attr : cur.getAttrs()) {
                        if (!continueSearch) return;
                        auto cursor2 = cur.getAttr(attr);
                        auto attrPath2(attrPath);
                        attrPath2.push_back(attr);
                        visit(*cursor2, attrPath2, false);
                    }
                };

                // Check isDerivation with exception handling for broken packages
                bool isDrv = false;
                try {
                    isDrv = cur.isDerivation();
                } catch (std::exception &) {
                    return;  // Skip packages that throw when checking type
                }

                if (isDrv) {
                    // Get package name attribute
                    auto nameAttr = cur.maybeGetAttr(state.s.name);
                    if (!nameAttr) return;

                    // Get name string with exception handling for broken packages
                    std::string nameStr;
                    try {
                        nameStr = nameAttr->getString();
                    } catch (std::exception &) {
                        return;  // Skip packages that throw when getting name
                    }
                    nix::DrvName drvName(nameStr);

                    // Get description from meta.description
                    std::string description;
                    auto metaAttr = cur.maybeGetAttr(state.s.meta);
                    if (metaAttr) {
                        auto descAttr = metaAttr->maybeGetAttr(state.s.description);
                        if (descAttr) {
                            try {
                                description = descAttr->getString();
                                // Normalize description (replace newlines with spaces)
                                std::replace(description.begin(), description.end(), '\n', ' ');
                            } catch (std::exception &) {
                                // Skip description if it throws
                            }
                        }
                    }

                    // Build attribute path string
                    std::string attrPathStr;
                    for (size_t i = 0; i < attrPathS.size(); i++) {
                        if (i > 0) attrPathStr += ".";
                        attrPathStr += attrPathS[i];
                    }

                    // Check exclude patterns first
                    for (auto & regex : p->excludeRegexes) {
                        if (std::regex_search(attrPathStr, regex) ||
                            std::regex_search(drvName.name, regex) ||
                            std::regex_search(description, regex)) {
                            return;  // Excluded
                        }
                    }

                    // Check include patterns (must match ALL - AND logic)
                    bool found = true;
                    if (!p->includeRegexes.empty()) {
                        for (auto & regex : p->includeRegexes) {
                            if (!std::regex_search(attrPathStr, regex) &&
                                !std::regex_search(drvName.name, regex) &&
                                !std::regex_search(description, regex)) {
                                found = false;
                                break;
                            }
                        }
                    }

                    if (found) {
                        nix_search_result result;
                        result.attr_path = attrPathStr.c_str();
                        result.name = drvName.name.c_str();
                        result.version = drvName.version.c_str();
                        result.description = description.c_str();

                        continueSearch = callback(&result, user_data);
                    }
                }
                else if (attrPath.size() == 0) {
                    // Root level - always recurse
                    recurse();
                }
                else if ((attrPathS[0] == "legacyPackages" && attrPath.size() <= 2) ||
                         (attrPathS[0] == "packages" && attrPath.size() <= 2)) {
                    // First two levels of packages/legacyPackages - always recurse
                    recurse();
                }
                else if (initialRecurse) {
                    // Initial recurse flag set - recurse once
                    recurse();
                }
                else if (attrPathS[0] == "legacyPackages" && attrPath.size() > 2) {
                    // Deeper legacyPackages - check recurseForDerivations
                    auto attr = cur.maybeGetAttr(state.s.recurseForDerivations);
                    if (attr && attr->getBool()) {
                        recurse();
                    }
                }

            } catch (nix::EvalError & e) {
                // Silently ignore eval errors in legacyPackages (like nix search does)
                if (!(attrPath.size() > 0 && attrPathS[0] == "legacyPackages")) {
                    throw;
                }
            }
        };

        // Start the search from the cursor
        auto initialPath = cursor->cursor->getAttrPath();
        visit(*cursor->cursor, initialPath, true);

        return NIX_OK;
    } NIXC_CATCH_ERRS
}
