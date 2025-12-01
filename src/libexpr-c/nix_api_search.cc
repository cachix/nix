#include "nix_api_search.h"
#include "nix_api_attr_cursor.h"
#include "nix_api_attr_cursor_internal.h"
#include "nix_api_util_internal.h"

#include "nix/expr/search.hh"

#include <cstdio>
#include <regex>
#include <string>
#include <vector>

struct nix_search_params {
    std::vector<std::regex> includeRegexes;
    std::vector<std::regex> excludeRegexes;
};

nix_search_params * nix_search_params_new(nix_c_context * context)
{
    if (context)
        nix_clear_err(context);
    try {
        return new nix_search_params();
    }
    NIXC_CATCH_ERRS_NULL
}

void nix_search_params_free(nix_search_params * params)
{
    delete params;
}

nix_err nix_search_params_add_regex(nix_c_context * context, nix_search_params * params, const char * pattern)
{
    if (context)
        nix_clear_err(context);
    try {
        params->includeRegexes.emplace_back(pattern, std::regex::extended | std::regex::icase);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_search_params_add_exclude(nix_c_context * context, nix_search_params * params, const char * pattern)
{
    if (context)
        nix_clear_err(context);
    try {
        params->excludeRegexes.emplace_back(pattern, std::regex::extended | std::regex::icase);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_search(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    nix_search_params * params,
    nix_search_callback callback,
    void * user_data)
{
    fprintf(stderr, "DEBUG: nix_search called, cursor=%p, params=%p, callback=%p\n",
            (void*)cursor, (void*)params, (void*)callback);
    if (context)
        nix_clear_err(context);
    try {
        fprintf(stderr, "DEBUG: accessing cursor->cache\n");
        auto & state = cursor->cache->state;
        fprintf(stderr, "DEBUG: got state\n");

        // Use empty params if none provided
        nix_search_params defaultParams;
        nix_search_params * p = params ? params : &defaultParams;

        bool continueSearch = true;
        int matchCount = 0;

        fprintf(stderr, "DEBUG: calling searchDerivations\n");
        nix::searchDerivations(
            state, *cursor->cursor, p->includeRegexes, p->excludeRegexes,
            [&](const nix::SearchMatch & match) {
                matchCount++;
                fprintf(stderr, "DEBUG: match %d: %s\n", matchCount, match.attrPath.c_str());
                if (!continueSearch)
                    return false;

                nix_search_result result;
                result.attr_path = match.attrPath.c_str();
                result.name = match.pname.c_str();
                result.version = match.version.c_str();
                result.description = match.description.c_str();

                continueSearch = callback(&result, user_data);
                return continueSearch;
            },
            1 // recurseDepth - default for generic expressions
        );

        fprintf(stderr, "DEBUG: searchDerivations completed, total matches: %d\n", matchCount);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}
