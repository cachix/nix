#ifndef NIX_API_FETCHERS_H
#define NIX_API_FETCHERS_H
/** @defgroup libfetchers libfetchers
 * @brief Bindings to the Nix fetchers library
 * @{
 */
/** @file
 * @brief Main entry for the libfetchers C bindings
 */

#include "nix_api_util.h"

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

// Type definitions
/**
 * @brief Shared settings object
 */
typedef struct nix_fetchers_settings nix_fetchers_settings;

/**
 * @brief Create a new fetchers settings object, pre-populated from nix.conf.
 *
 * Settings are automatically loaded from:
 * - System config: /etc/nix/nix.conf (or NIX_CONF_DIR)
 * - User config files (NIX_USER_CONF_FILES or XDG config directories)
 * - Environment variable NIX_CONFIG
 *
 * This includes settings like access-tokens for authenticated GitHub/GitLab access.
 *
 * @note Requires nix_libstore_init() to have been called first.
 *
 * @param[out] context Optional, stores error information
 * @return A new fetchers settings object, or NULL on error
 */
nix_fetchers_settings * nix_fetchers_settings_new(nix_c_context * context);

void nix_fetchers_settings_free(nix_fetchers_settings * settings);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NIX_API_FETCHERS_H