#ifndef NIX_API_STORE_H
#define NIX_API_STORE_H
/**
 * @defgroup libstore libstore
 * @brief C bindings for nix libstore
 *
 * libstore is used for talking to a Nix store
 * @{
 */
/** @file
 * @brief Main entry for the libstore C bindings
 */

#include "nix_api_util.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/** @brief Reference to a Nix store */
typedef struct Store Store;
/** @brief Nix store path */
typedef struct StorePath StorePath;
/** @brief Nix Derivation */
typedef struct nix_derivation nix_derivation;

/**
 * @brief Initializes the Nix store library
 *
 * This function should be called before creating a Store
 * This function can be called multiple times.
 *
 * @param[out] context Optional, stores error information
 * @return NIX_OK if the initialization was successful, an error code otherwise.
 */
nix_err nix_libstore_init(nix_c_context * context);

/**
 * @brief Like nix_libstore_init, but does not load the Nix configuration.
 *
 * This is useful when external configuration is not desired, such as when running unit tests.
 */
nix_err nix_libstore_init_no_load_config(nix_c_context * context);

/**
 * @brief Open a nix store.
 *
 * Store instances may share state and resources behind the scenes.
 *
 * @param[out] context Optional, stores error information
 *
 * @param[in] uri @parblock
 *   URI of the Nix store, copied.
 *
 *   If `NULL`, the store from the settings will be used.
 *   Note that `"auto"` holds a strange middle ground, reading part of the general environment, but not all of it. It
 * ignores `NIX_REMOTE` and the `store` option. For this reason, `NULL` is most likely the better choice.
 *
 *   For supported store URLs, see [*Store URL format* in the Nix Reference
 * Manual](https://nix.dev/manual/nix/stable/store/types/#store-url-format).
 * @endparblock
 *
 * @param[in] params @parblock
 *   optional, null-terminated array of key-value pairs, e.g. {{"endpoint",
 * "https://s3.local"}}.
 *
 *   See [*Store Types* in the Nix Reference Manual](https://nix.dev/manual/nix/stable/store/types).
 * @endparblock
 *
 * @return a Store pointer, NULL in case of errors
 *
 * @see nix_store_free
 */
Store * nix_store_open(nix_c_context * context, const char * uri, const char *** params);

/**
 * @brief Deallocate a nix store and free any resources if not also held by other Store instances.
 *
 * Does not fail.
 *
 * @param[in] store the store to free
 */
void nix_store_free(Store * store);

/**
 * @brief get the URI of a nix store
 * @param[out] context Optional, stores error information
 * @param[in] store nix store reference
 * @param[in] callback Called with the URI.
 * @param[in] user_data optional, arbitrary data, passed to the callback when it's called.
 * @see nix_get_string_callback
 * @return error code, NIX_OK on success.
 */
nix_err nix_store_get_uri(nix_c_context * context, Store * store, nix_get_string_callback callback, void * user_data);

/**
 * @brief get the storeDir of a Nix store, typically `"/nix/store"`
 * @param[out] context Optional, stores error information
 * @param[in] store nix store reference
 * @param[in] callback Called with the URI.
 * @param[in] user_data optional, arbitrary data, passed to the callback when it's called.
 * @see nix_get_string_callback
 * @return error code, NIX_OK on success.
 */
nix_err
nix_store_get_storedir(nix_c_context * context, Store * store, nix_get_string_callback callback, void * user_data);

/**
 * @brief Parse a Nix store path into a StorePath
 *
 * @note Don't forget to free this path using nix_store_path_free()!
 * @param[out] context Optional, stores error information
 * @param[in] store nix store reference
 * @param[in] path Path string to parse, copied
 * @return owned store path, NULL on error
 */
StorePath * nix_store_parse_path(nix_c_context * context, Store * store, const char * path);

/**
 * @brief Get the path name (e.g. "name" in /nix/store/...-name)
 *
 * @param[in] store_path the path to get the name from
 * @param[in] callback called with the name
 * @param[in] user_data arbitrary data, passed to the callback when it's called.
 */
void nix_store_path_name(const StorePath * store_path, nix_get_string_callback callback, void * user_data);

/**
 * @brief Copy a StorePath
 *
 * @param[in] p the path to copy
 * @return a new StorePath
 */
StorePath * nix_store_path_clone(const StorePath * p);

/** @brief Deallocate a StorePath
 *
 * Does not fail.
 * @param[in] p the path to free
 */
void nix_store_path_free(StorePath * p);

/**
 * @brief Check if a StorePath is valid (i.e. that corresponding store object and its closure of references exists in
 * the store)
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference
 * @param[in] path Path to check
 * @return true or false, error info in context
 */
bool nix_store_is_valid_path(nix_c_context * context, Store * store, const StorePath * path);

/**
 * @brief Get the physical location of a store path
 *
 * A store may reside at a different location than its `storeDir` suggests.
 * This situation is called a relocated store.
 * Relocated stores are used during NixOS installation, as well as in restricted computing environments that don't offer
 * a writable `/nix/store`.
 *
 * Not all types of stores support this operation.
 *
 * @param[in] context Optional, stores error information
 * @param[in] store nix store reference
 * @param[in] path the path to get the real path from
 * @param[in] callback called with the real path
 * @param[in] user_data arbitrary data, passed to the callback when it's called.
 */
nix_err nix_store_real_path(
    nix_c_context * context, Store * store, StorePath * path, nix_get_string_callback callback, void * user_data);

// nix_err nix_store_ensure(Store*, const char*);
// nix_err nix_store_build_paths(Store*);
/**
 * @brief Realise a Nix store path
 *
 * Blocking, calls callback once for each realised output.
 *
 * @note When working with expressions, consider using e.g. nix_string_realise to get the output. `.drvPath` may not be
 * accurate or available in the future. See https://github.com/NixOS/nix/issues/6507
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference
 * @param[in] path Path to build
 * @param[in] userdata data to pass to every callback invocation
 * @param[in] callback called for every realised output
 * @return NIX_OK if the build succeeded, or an error code if the build/scheduling/outputs/copying/etc failed.
 *         On error, the callback is never invoked and error information is stored in context.
 */
nix_err nix_store_realise(
    nix_c_context * context,
    Store * store,
    StorePath * path,
    void * userdata,
    void (*callback)(void * userdata, const char * outname, const StorePath * out));

/**
 * @brief get the version of a nix store.
 *
 * If the store doesn't have a version (like the dummy store), returns an empty string.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store nix store reference
 * @param[in] callback Called with the version.
 * @param[in] user_data optional, arbitrary data, passed to the callback when it's called.
 * @see nix_get_string_callback
 * @return error code, NIX_OK on success.
 */
nix_err
nix_store_get_version(nix_c_context * context, Store * store, nix_get_string_callback callback, void * user_data);

/**
 * @brief Create a `nix_derivation` from a JSON representation of that derivation.
 *
 * @param[out] context Optional, stores error information.
 * @param[in] store nix store reference.
 * @param[in] json JSON of the derivation as a string.
 */
nix_derivation * nix_derivation_from_json(nix_c_context * context, Store * store, const char * json);

/**
 * @brief Add the given `nix_derivation` to the given store
 *
 * @param[out] context Optional, stores error information.
 * @param[in] store nix store reference. The derivation will be inserted here.
 * @param[in] derivation nix_derivation to insert into the given store.
 */
StorePath * nix_add_derivation(nix_c_context * context, Store * store, nix_derivation * derivation);

/**
 * @brief Deallocate a `nix_derivation`
 *
 * Does not fail.
 * @param[in] drv the derivation to free
 */
void nix_derivation_free(nix_derivation * drv);

/**
 * @brief Copy the closure of `path` from `srcStore` to `dstStore`.
 *
 * @param[out] context Optional, stores error information
 * @param[in] srcStore nix source store reference
 * @param[in] dstStore nix destination store reference
 * @param[in] path Path to copy
 */
nix_err nix_store_copy_closure(nix_c_context * context, Store * srcStore, Store * dstStore, StorePath * path);

/**
 * @brief Gets the closure of a specific store path
 *
 * @note The callback borrows each StorePath only for the duration of the call.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store nix store reference
 * @param[in] store_path The path to compute from
 * @param[in] flip_direction If false, compute the forward closure (paths referenced by any store path in the closure).
 *                           If true, compute the backward closure (paths that reference any store path in the closure).
 * @param[in] include_outputs If flip_direction is false: for any derivation in the closure, include its outputs.
 *                            If flip_direction is true: for any output in the closure, include derivations that produce
 *                            it.
 * @param[in] include_derivers If flip_direction is false: for any output in the closure, include the derivation that
 *                             produced it.
 *                             If flip_direction is true: for any derivation in the closure, include its outputs.
 * @param[in] callback The function to call for every store path, in no particular order
 * @param[in] userdata The userdata to pass to the callback
 */
nix_err nix_store_get_fs_closure(
    nix_c_context * context,
    Store * store,
    const StorePath * store_path,
    bool flip_direction,
    bool include_outputs,
    bool include_derivers,
    void * userdata,
    void (*callback)(nix_c_context * context, void * userdata, const StorePath * store_path));

/**
 * @brief Add a substituter to a store at runtime.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference
 * @param[in] uri The substituter URI (e.g., "https://cache.nixos.org")
 * @return NIX_OK on success, or an error code on failure
 */
nix_err nix_store_add_substituter(nix_c_context * context, Store * store, const char * uri);

/**
 * @brief Remove a substituter from a store.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference
 * @param[in] uri The substituter URI to remove
 * @return NIX_OK on success, or NIX_ERR_KEY if not found
 */
nix_err nix_store_remove_substituter(nix_c_context * context, Store * store, const char * uri);

/**
 * @brief Callback type for listing substituters
 *
 * @param[in] uri The substituter URI
 * @param[in] priority The priority value of this substituter
 * @param[in] user_data User-provided data passed to nix_store_list_substituters
 */
typedef void (*nix_substituter_callback)(const char * uri, int priority, void * user_data);

/**
 * @brief Get all substituters for a store.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference
 * @param[in] callback Function called for each substituter
 * @param[in] user_data Data passed to callback
 * @return NIX_OK on success, or an error code
 */
nix_err nix_store_list_substituters(
    nix_c_context * context, Store * store, nix_substituter_callback callback, void * user_data);

/**
 * @brief Clear all substituters from a store.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference
 * @return NIX_OK on success
 */
nix_err nix_store_clear_substituters(nix_c_context * context, Store * store);

/**
 * @brief Add a permanent GC root for a store path.
 *
 * Creates a symlink at `gc_root` that points to `path`, and registers it as
 * a GC root so the path will not be garbage collected.
 *
 * This only works with LocalFSStore or derived store types.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference (must be a local filesystem store)
 * @param[in] path The store path to root
 * @param[in] gc_root The filesystem path where the GC root symlink will be created
 * @return NIX_OK on success, or an error code on failure
 */
nix_err nix_store_add_perm_root(nix_c_context * context, Store * store, StorePath * path, const char * gc_root);

/**
 * @brief Add an indirect GC root for a store path.
 *
 * Adds an indirect (weak) reference GC root that points to `symlink_path`.
 * This is used internally by `nix_store_add_perm_root` on stores that support it.
 *
 * This only works with IndirectRootStore or LocalStore (stores that support indirect roots).
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference (must support indirect roots)
 * @param[in] symlink_path The filesystem path to the symlink created by add_perm_root
 * @return NIX_OK on success, or an error code on failure
 */
nix_err nix_store_add_indirect_root(nix_c_context * context, Store * store, const char * symlink_path);

/**
 * @brief Delete a store path.
 *
 * Deletes the store path and all its contents. The path must be unreachable
 * (i.e., not referenced by any GC root).
 *
 * This only works with LocalStore.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference (must be LocalStore)
 * @param[in] path The store path to delete (filesystem path)
 * @param[out] bytes_freed Pointer to uint64_t that will be set to the number of bytes freed.
 *                         Can be NULL if you don't care about this information.
 * @return NIX_OK on success, or an error code on failure
 */
nix_err nix_store_delete_path(nix_c_context * context, Store * store, const char * path, uint64_t * bytes_freed);

/**
 * @brief Callback for iterating over store paths
 *
 * Called once for each store path in a result set.
 *
 * @param[in] path The store path
 * @param[in] user_data User-provided data passed to the function
 */
typedef void (*nix_store_path_callback)(const StorePath * path, void * user_data);

/**
 * @brief Compute the filesystem closure of store paths.
 *
 * The closure is the set of all paths reachable from the input paths
 * through references. This function is useful for determining all dependencies
 * of given paths before performing operations on them.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix Store reference
 * @param[in] paths Array of starting store paths (cannot be NULL if num_paths > 0)
 * @param[in] num_paths Number of paths in the array
 * @param[in] flip_direction If true, compute reverse dependencies (dependents) instead of forward
 * @param[in] include_outputs If true, include outputs of derivations
 * @param[in] include_derivers If true, include derivers of store paths
 * @param[in] callback Function called for each path in the computed closure
 * @param[in] user_data Arbitrary data passed to the callback
 * @return NIX_OK on success, error code on failure
 */
nix_err nix_store_compute_fs_closure(
    nix_c_context * context,
    Store * store,
    StorePath ** paths,
    size_t num_paths,
    bool flip_direction,
    bool include_outputs,
    bool include_derivers,
    nix_store_path_callback callback,
    void * user_data);

// cffi end
#ifdef __cplusplus
}
#endif
/**
 * @}
 */
#endif // NIX_API_STORE_H
