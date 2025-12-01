#include "nix_api_search.h"
#include "nix_api_attr_cursor.h"
#include "nix_api_attr_cursor_internal.h"
#include "nix_api_util_internal.h"

#include "nix/store/names.hh"

#include <algorithm>
#include <cstdio>
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
    fprintf(stderr, "DEBUG C++: nix_search called, cursor=%p\n", (void*)cursor);
    if (context) nix_clear_err(context);
    try {
        fprintf(stderr, "DEBUG C++: accessing cursor->cache, cache=%p\n", (void*)cursor->cache);
        auto & state = cursor->cache->state;
        fprintf(stderr, "DEBUG C++: got state reference\n");

        // Use empty params if none provided
        nix_search_params defaultParams;
        nix_search_params * p = params ? params : &defaultParams;
        fprintf(stderr, "DEBUG C++: params setup done\n");

        // Track whether to continue searching
        bool continueSearch = true;

        // Recursive visit function (mirrors search.cc logic)
        std::function<void(nix::eval_cache::AttrCursor &, const std::vector<nix::Symbol> &, bool)> visit;
        fprintf(stderr, "DEBUG C++: about to define visit lambda\n");

        visit = [&](nix::eval_cache::AttrCursor & cur, const std::vector<nix::Symbol> & attrPath, bool initialRecurse) {
            fprintf(stderr, "DEBUG C++: visit called, attrPath.size()=%zu, initialRecurse=%d\n", attrPath.size(), initialRecurse);
            if (!continueSearch) return;

            fprintf(stderr, "DEBUG C++: resolving attrPath\n");
            auto attrPathS = state.symbols.resolve(attrPath);
            std::string pathStr;
            for (size_t i = 0; i < attrPathS.size(); i++) {
                if (i > 0) pathStr += ".";
                pathStr += attrPathS[i];
            }
            fprintf(stderr, "DEBUG C++: attrPath resolved to '%s', checking isDerivation\n", pathStr.c_str());

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

                bool isDrv = false;
                try {
                    isDrv = cur.isDerivation();
                } catch (std::exception & e) {
                    fprintf(stderr, "DEBUG C++: isDerivation threw: %s\n", e.what());
                    return;
                }
                fprintf(stderr, "DEBUG C++: isDerivation=%d\n", isDrv);
                if (isDrv) {
                    // Get package name attribute
                    fprintf(stderr, "DEBUG C++: is derivation, getting name\n");
                    auto nameAttr = cur.maybeGetAttr(state.s.name);
                    if (!nameAttr) {
                        fprintf(stderr, "DEBUG C++: no name attr, returning\n");
                        return;
                    }
                    fprintf(stderr, "DEBUG C++: got name attr, getting string\n");
                    auto nameStr = nameAttr->getString();
                    fprintf(stderr, "DEBUG C++: name string = %s\n", nameStr.c_str());
                    nix::DrvName drvName(nameStr);

                    // Get description from meta.description
                    std::string description;
                    auto metaAttr = cur.maybeGetAttr(state.s.meta);
                    if (metaAttr) {
                        auto descAttr = metaAttr->maybeGetAttr(state.s.description);
                        if (descAttr) {
                            description = descAttr->getString();
                            // Normalize description (replace newlines with spaces)
                            std::replace(description.begin(), description.end(), '\n', ' ');
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
                        fprintf(stderr, "DEBUG C++: found match, calling callback for %s\n", attrPathStr.c_str());
                        nix_search_result result;
                        result.attr_path = attrPathStr.c_str();
                        result.name = drvName.name.c_str();
                        result.version = drvName.version.c_str();
                        result.description = description.c_str();

                        continueSearch = callback(&result, user_data);
                        fprintf(stderr, "DEBUG C++: callback returned %d\n", continueSearch);
                    } else {
                        fprintf(stderr, "DEBUG C++: derivation %s did not match patterns\n", attrPathStr.c_str());
                    }
                }
                else if (attrPath.size() == 0) {
                    // Root level - always recurse
                    fprintf(stderr, "DEBUG C++: root level, recursing\n");
                    recurse();
                    fprintf(stderr, "DEBUG C++: root level recurse done\n");
                }
                else if ((attrPathS[0] == "legacyPackages" && attrPath.size() <= 2) ||
                         (attrPathS[0] == "packages" && attrPath.size() <= 2)) {
                    // First two levels of packages/legacyPackages - always recurse
                    fprintf(stderr, "DEBUG C++: packages level, recursing\n");
                    recurse();
                }
                else if (initialRecurse) {
                    // Initial recurse flag set - recurse once
                    fprintf(stderr, "DEBUG C++: initial recurse\n");
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
        fprintf(stderr, "DEBUG C++: about to call visit, cursor->cursor=%p\n", (void*)cursor->cursor.get());
        auto initialPath = cursor->cursor->getAttrPath();
        fprintf(stderr, "DEBUG C++: got initial path, size=%zu\n", initialPath.size());
        visit(*cursor->cursor, initialPath, true);
        fprintf(stderr, "DEBUG C++: visit returned\n");

        return NIX_OK;
    } NIXC_CATCH_ERRS
}
