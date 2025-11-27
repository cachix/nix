#ifndef NIX_API_ATTR_CURSOR_H
#define NIX_API_ATTR_CURSOR_H
/** @file
 * @brief C bindings for evaluation cache and lazy attribute set traversal
 *
 * This API wraps the C++ eval_cache::EvalCache and eval_cache::AttrCursor
 * classes, providing lazy attribute traversal with optional SQLite caching.
 */

#include "nix_api_expr.h"
#include "nix_api_util.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/**
 * @brief Evaluation cache for lazy attribute evaluation with optional SQLite backing.
 *
 * Wraps nix::eval_cache::EvalCache. Create with nix_eval_cache_create(),
 * then obtain the root cursor with nix_eval_cache_get_root().
 */
typedef struct nix_eval_cache nix_eval_cache;

/**
 * @brief Cursor for lazy traversal of attribute sets.
 *
 * Wraps nix::eval_cache::AttrCursor. Cursors are obtained from
 * nix_eval_cache_get_root() or nix_attr_cursor_get_attr().
 */
typedef struct nix_attr_cursor nix_attr_cursor;

/**
 * @brief Create an evaluation cache from a root value.
 *
 * Creates an EvalCache without flake dependencies. The cache can optionally
 * persist evaluation results to SQLite for faster subsequent access.
 *
 * @param[out] context Optional, stores error information
 * @param[in] state The EvalState
 * @param[in] root_value The root value to cache (usually an attrset)
 * @param[in] cache_key Optional SHA256 hash string for cache fingerprint.
 *                      If NULL, caching is disabled. If provided, results
 *                      are cached to ~/.cache/nix/eval-cache-v6/<hash>.sqlite
 * @return A new eval cache, or NULL on error. Caller must free with nix_eval_cache_free.
 */
nix_eval_cache * nix_eval_cache_create(
    nix_c_context * context,
    EvalState * state,
    nix_value * root_value,
    const char * cache_key);

/**
 * @brief Free an evaluation cache.
 *
 * Also invalidates any cursors obtained from this cache.
 */
void nix_eval_cache_free(nix_eval_cache * cache);

/**
 * @brief Get the root cursor from an evaluation cache.
 *
 * @param[out] context Optional, stores error information
 * @param[in] cache The evaluation cache
 * @return A new cursor at the root, or NULL on error. Caller must free with nix_attr_cursor_free.
 */
nix_attr_cursor * nix_eval_cache_get_root(
    nix_c_context * context,
    nix_eval_cache * cache);

/**
 * @brief Free an attribute cursor.
 */
void nix_attr_cursor_free(nix_attr_cursor * cursor);

/**
 * @brief Get child attribute by name, returning NULL if not found.
 *
 * @param[out] context Optional, stores error information
 * @param[in] cursor The parent cursor
 * @param[in] name The attribute name to look up
 * @return A new child cursor, or NULL if not found or on error.
 *         Caller must free with nix_attr_cursor_free.
 */
nix_attr_cursor * nix_attr_cursor_get_attr(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    const char * name);

/**
 * @brief Get number of attributes in the current attrset.
 *
 * @param[out] context Optional, stores error information
 * @param[in] cursor The cursor
 * @return Number of attributes, or -1 on error.
 */
int64_t nix_attr_cursor_get_attrs_count(nix_c_context * context, nix_attr_cursor * cursor);

/**
 * @brief Get attribute name by index.
 *
 * @param[out] context Optional, stores error information
 * @param[in] cursor The cursor
 * @param[in] index The attribute index (0-based)
 * @param[in] callback Called with the attribute name
 * @param[in] user_data Passed to callback
 * @return NIX_OK on success, error code otherwise.
 */
nix_err nix_attr_cursor_get_attr_name(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    unsigned int index,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Check if cursor points to a derivation.
 *
 * A derivation has attribute `type = "derivation"`.
 *
 * @param[out] context Optional, stores error information
 * @param[in] cursor The cursor
 * @param[out] is_drv Set to true if this is a derivation
 * @return NIX_OK on success, error code otherwise.
 */
nix_err nix_attr_cursor_is_derivation(nix_c_context * context, nix_attr_cursor * cursor, bool * is_drv);

/**
 * @brief Get string value at cursor position.
 *
 * @param[out] context Optional, stores error information
 * @param[in] cursor The cursor
 * @param[in] callback Called with the string value
 * @param[in] user_data Passed to callback
 * @return NIX_OK on success, error code otherwise.
 */
nix_err nix_attr_cursor_get_string(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Get bool value at cursor position.
 *
 * @param[out] context Optional, stores error information
 * @param[in] cursor The cursor
 * @param[out] value Set to the bool value
 * @return NIX_OK on success, error code otherwise.
 */
nix_err nix_attr_cursor_get_bool(nix_c_context * context, nix_attr_cursor * cursor, bool * value);

// cffi end
#ifdef __cplusplus
}
#endif

#endif // NIX_API_ATTR_CURSOR_H
