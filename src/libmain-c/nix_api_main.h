#ifndef NIX_API_MAIN_H
#define NIX_API_MAIN_H
/**
 * @defgroup libmain libmain
 * @brief C bindings for nix libmain
 *
 * libmain has misc utilities for CLI commands
 * @{
 */
/** @file
 * @brief Main entry for the libmain C bindings
 */

#include "nix_api_util.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/**
 * @brief Loads the plugins specified in Nix's plugin-files setting.
 *
 * Call this once, after calling your desired init functions and setting
 * relevant settings.
 *
 * @param[out] context Optional, stores error information
 * @return NIX_OK if the initialization was successful, an error code otherwise.
 */
nix_err nix_init_plugins(nix_c_context * context);

/**
 * @brief Sets the log format
 *
 * @param[out] context Optional, stores error information
 * @param[in] format The string name of the format.
 */
nix_err nix_set_log_format(nix_c_context * context, const char * format);

/**
 * @brief Trigger an interrupt
 *
 * Sets the interrupt flag and runs all registered interrupt callbacks.
 * This can be used to programmatically interrupt long-running Nix operations.
 *
 * @note On Windows, this only sets the interrupt flag; callbacks are not supported.
 */
void nix_trigger_interrupt(void);

// cffi end
#ifdef __cplusplus
}
#endif
/**
 * @}
 */
#endif // NIX_API_MAIN_H
