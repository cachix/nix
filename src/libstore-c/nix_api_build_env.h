#ifndef NIX_API_BUILD_ENV_H
#define NIX_API_BUILD_ENV_H

/**
 * @defgroup libstore libstore
 * @brief C bindings for nix libstore
 *
 * This module is part of libstore-c for working with build environments
 * extracted from derivations.
 * @{
 */

/** @file
 * @brief C bindings for BuildEnvironment functionality
 */

#include "nix_api_util.h"
#include "nix_api_store.h"
#include <stddef.h>

#ifndef __has_c_attribute
#  define __has_c_attribute(x) 0
#endif

#if __has_c_attribute(deprecated)
#  define NIX_DEPRECATED(msg) [[deprecated(msg)]]
#else
#  define NIX_DEPRECATED(msg)
#endif

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

// Type definitions

/**
 * @brief Represents a build environment extracted from a derivation.
 *
 * A build environment contains environment variables (in various forms),
 * bash function definitions, and optionally structured attributes.
 * It can be parsed from JSON and converted back to JSON or bash script format.
 */
typedef struct nix_build_env nix_build_env;

// Function prototypes

/**
 * @brief Create a new, empty BuildEnvironment.
 *
 * @param[out] context Optional, stores error information
 * @return A new BuildEnvironment, or NULL on error
 * @see nix_build_env_free
 */
nix_build_env * nix_build_env_new(nix_c_context * context);

/**
 * @brief Parse a BuildEnvironment from a JSON string.
 *
 * The JSON must have the structure produced by `nix print-dev-env --json`,
 * with "variables", "bashFunctions", and optionally "structuredAttrs" keys.
 *
 * @param[out] context Optional, stores error information
 * @param[in] json A null-terminated JSON string
 * @return A new BuildEnvironment parsed from the JSON, or NULL on error
 * @see nix_build_env_free
 */
nix_build_env * nix_build_env_parse_json(nix_c_context * context, const char * json);

/**
 * @brief Free a BuildEnvironment.
 *
 * Does not fail.
 *
 * @param[in] env The BuildEnvironment to free. May be NULL.
 */
void nix_build_env_free(nix_build_env * env);

/**
 * @brief Serialize a BuildEnvironment to JSON.
 *
 * The output JSON will have the same structure as accepted by
 * nix_build_env_parse_json.
 *
 * @param[out] context Optional, stores error information
 * @param[in] env The environment to serialize
 * @param[in] callback Called with chunks of the JSON output
 * @param[in] user_data Optional, arbitrary data passed to the callback
 * @return NIX_OK on success, an error code otherwise
 * @see nix_get_string_callback
 */
nix_err nix_build_env_to_json(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Serialize a BuildEnvironment to bash script format.
 *
 * Generates bash code that can be sourced to apply the build environment.
 * This includes variable assignments, export statements, and function definitions.
 *
 * @param[out] context Optional, stores error information
 * @param[in] env The environment to serialize
 * @param[in] callback Called with chunks of the bash output
 * @param[in] user_data Optional, arbitrary data passed to the callback
 * @return NIX_OK on success, an error code otherwise
 * @see nix_get_string_callback
 */
nix_err nix_build_env_to_bash(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Check if the environment provides structured attributes.
 *
 * @param[in] env The environment to check
 * @return true if structured attributes are available, false otherwise
 */
bool nix_build_env_has_structured_attrs(nix_build_env * env);

/**
 * @brief Get the structured attributes JSON content.
 *
 * Only valid if nix_build_env_has_structured_attrs returns true.
 *
 * @param[out] context Optional, stores error information
 * @param[in] env The environment
 * @param[in] callback Called with the JSON content
 * @param[in] user_data Optional, arbitrary data passed to the callback
 * @return NIX_OK on success, an error code otherwise
 * @see nix_get_string_callback
 */
nix_err nix_build_env_get_attrs_json(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Get the structured attributes shell script content.
 *
 * Only valid if nix_build_env_has_structured_attrs returns true.
 *
 * @param[out] context Optional, stores error information
 * @param[in] env The environment
 * @param[in] callback Called with the shell script content
 * @param[in] user_data Optional, arbitrary data passed to the callback
 * @return NIX_OK on success, an error code otherwise
 * @see nix_get_string_callback
 */
nix_err nix_build_env_get_attrs_sh(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Extract a BuildEnvironment from a store derivation.
 *
 * Given a derivation store path, extract the build environment that would be
 * applied when building that derivation. This reads the .drv file and creates
 * a BuildEnvironment with the variables and functions defined for the build.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store Nix store reference
 * @param[in] drv_path The derivation store path to extract environment from
 * @return A new BuildEnvironment extracted from the derivation, or NULL on error
 * @see nix_build_env_free, nix_store_parse_path
 */
nix_build_env * nix_build_env_from_derivation(
    nix_c_context * context,
    Store * store,
    const StorePath * drv_path);

// cffi end
#ifdef __cplusplus
}
#endif

/** @} */
#endif // NIX_API_BUILD_ENV_H
