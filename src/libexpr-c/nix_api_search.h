#ifndef NIX_API_SEARCH_H
#define NIX_API_SEARCH_H
/** @file
 * @brief C bindings for nix search functionality
 *
 * This API provides package search functionality similar to `nix search`,
 * allowing traversal of attribute sets to find derivations matching
 * specified patterns.
 */

#include "nix_api_attr_cursor.h"
#include "nix_api_util.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/**
 * @brief Search result passed to callback.
 *
 * All string pointers are valid only during the callback invocation.
 */
typedef struct {
    const char * attr_path;   /**< Full attribute path, e.g. "legacyPackages.x86_64-linux.hello" */
    const char * name;        /**< Package name, e.g. "hello" */
    const char * version;     /**< Package version, e.g. "2.10" (may be empty) */
    const char * description; /**< Package description (may be empty) */
} nix_search_result;

/**
 * @brief Callback invoked for each search result.
 *
 * @param result The search result (valid only during callback)
 * @param user_data User-provided context
 * @return true to continue searching, false to stop early
 */
typedef bool (*nix_search_callback)(const nix_search_result * result, void * user_data);

/**
 * @brief Search parameters configuration.
 */
typedef struct nix_search_params nix_search_params;

/**
 * @brief Create search parameters with default settings.
 *
 * Default settings:
 * - No include patterns (matches all)
 * - No exclude patterns
 * - recurseForDerivations logic enabled (for legacyPackages compatibility)
 *
 * @param[out] context Optional, stores error information
 * @return New search params, or NULL on error. Caller must free with nix_search_params_free.
 */
nix_search_params * nix_search_params_new(nix_c_context * context);

/**
 * @brief Free search parameters.
 */
void nix_search_params_free(nix_search_params * params);

/**
 * @brief Add an include regex pattern.
 *
 * Results must match ALL include patterns (AND logic).
 * Patterns are matched case-insensitively against:
 * - attribute path
 * - package name
 * - description
 *
 * Uses extended POSIX regex syntax.
 *
 * @param[out] context Optional, stores error information
 * @param[in] params Search parameters
 * @param[in] pattern Regex pattern to add
 * @return NIX_OK on success, error code otherwise.
 */
nix_err nix_search_params_add_regex(
    nix_c_context * context,
    nix_search_params * params,
    const char * pattern);

/**
 * @brief Add an exclude regex pattern.
 *
 * Results matching ANY exclude pattern are filtered out.
 * Patterns are matched case-insensitively against:
 * - attribute path
 * - package name
 * - description
 *
 * Uses extended POSIX regex syntax.
 *
 * @param[out] context Optional, stores error information
 * @param[in] params Search parameters
 * @param[in] pattern Regex pattern to add
 * @return NIX_OK on success, error code otherwise.
 */
nix_err nix_search_params_add_exclude(
    nix_c_context * context,
    nix_search_params * params,
    const char * pattern);

/**
 * @brief Perform a search over an attribute set.
 *
 * Traverses the attrset starting from cursor, finding derivations that
 * match the search criteria and invoking the callback for each match.
 *
 * The traversal follows `nix search` semantics:
 * - Always recurses into the root level
 * - Always recurses 2 levels deep into `packages.*` and `legacyPackages.*`
 * - For deeper `legacyPackages` levels, only recurses if `recurseForDerivations = true`
 * - Evaluation errors in `legacyPackages` are silently ignored
 *
 * @param[out] context Optional, stores error information
 * @param[in] cursor Root cursor to search from
 * @param[in] params Search parameters (regexes, settings). Pass NULL for defaults.
 * @param[in] callback Function called for each result
 * @param[in] user_data Passed to callback
 * @return NIX_OK on success, error code otherwise.
 */
nix_err nix_search(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    nix_search_params * params,
    nix_search_callback callback,
    void * user_data);

// cffi end
#ifdef __cplusplus
}
#endif

#endif // NIX_API_SEARCH_H
