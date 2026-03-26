#ifndef NIX_API_STORE_PATH_INFO_H
#define NIX_API_STORE_PATH_INFO_H
/**
 * @defgroup libstore_pathinfo PathInfo
 * @ingroup libstore
 * @brief Store path metadata queries
 * @{
 */
/** @file
 * @brief Functions for querying store object metadata (path info)
 */

#include "nix_api_util.h"
#include "nix_api_store/store_path.h"

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/** @brief Reference to a Nix store (forward declaration) */
typedef struct Store Store;

/**
 * @brief JSON format version for path info serialization.
 *
 * Controls the shape of the JSON returned by nix_store_query_path_info_json().
 *
 * @note Should be kept in sync with nix::PathInfoJsonFormat in path-info.hh.
 */
typedef enum {
    /**
     * Legacy format.
     * - narHash: SRI string (e.g. "sha256-...")
     * - references: full store paths (e.g. "/nix/store/...")
     * - ca: rendered string or null
     * - signatures: array of strings
     */
    NIX_PATH_INFO_JSON_FORMAT_V1 = 1,
    /**
     * Structured format.
     * - narHash: structured object with algo and hash fields
     * - references: store path base names only
     * - ca: structured object or null
     * - signatures: array of strings
     */
    NIX_PATH_INFO_JSON_FORMAT_V2 = 2,
    /**
     * Like V2, but with structured signatures.
     * - signatures: array of objects with keyName and sig fields
     */
    NIX_PATH_INFO_JSON_FORMAT_V3 = 3,
} nix_path_info_json_format;

/**
 * @brief Query store path metadata and return it as JSON.
 *
 * Queries the store for metadata about the given path (narHash, narSize,
 * references, deriver, signatures, etc.) and returns the result as a JSON
 * string via the callback.
 *
 * The JSON includes both "pure" (intrinsic) and "impure" (store-specific)
 * fields. Impure fields include: deriver, registrationTime, ultimate, and
 * signatures. See the Nix manual's "Store Object Info" JSON schema for the
 * full specification.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix store reference
 * @param[in] store_path The store path to query (must be valid in the store)
 * @param[in] format JSON format version to use
 * @param[in] callback Called with the JSON string
 * @param[in] user_data arbitrary data, passed to the callback when it's called
 * @return NIX_OK on success, error code on failure
 */
nix_err nix_store_query_path_info_json(
    nix_c_context * context,
    Store * store,
    const StorePath * store_path,
    nix_path_info_json_format format,
    nix_get_string_callback callback,
    void * user_data);

// cffi end
#ifdef __cplusplus
}
#endif
/**
 * @}
 */
#endif // NIX_API_STORE_PATH_INFO_H
