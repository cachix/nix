#pragma once

#include "nix/expr/eval-cache.hh"

#include <functional>
#include <regex>
#include <string>
#include <vector>

namespace nix {

/**
 * Information about a matching derivation found during search.
 */
struct SearchMatch {
    std::string attrPath;
    std::string pname;
    std::string version;
    std::string description;
};

/**
 * Callback invoked for each matching derivation.
 * Return true to continue searching, false to stop.
 */
using SearchCallback = std::function<bool(const SearchMatch &)>;

/**
 * Search for derivations in an attribute set.
 *
 * Works with ANY Nix expression - not flake-specific.
 * Recursion rules:
 * - Always recurses for first `recurseDepth` levels
 * - At deeper levels: recurses if `recurseForDerivations = true`
 * - Derivations are matched against regexes
 *
 * @param state       EvalState for evaluation
 * @param cursor      Root cursor to search from
 * @param regexes     Include patterns (AND logic - must match all)
 * @param excludeRegexes  Exclude patterns (OR logic - excluded if any match)
 * @param callback    Called for each match, return false to stop
 * @param recurseDepth  Auto-recurse this many levels before checking recurseForDerivations (default: 1)
 */
void searchDerivations(
    EvalState & state,
    eval_cache::AttrCursor & cursor,
    const std::vector<std::regex> & regexes,
    const std::vector<std::regex> & excludeRegexes,
    SearchCallback callback,
    size_t recurseDepth = 1
);

} // namespace nix
