#ifndef NIX_API_LOGGER_H
#define NIX_API_LOGGER_H
/**
 * @defgroup logger Activity and Logger callbacks
 * @brief C bindings for Nix's logging system
 *
 * Allows setting up callbacks to observe Nix's activities such as builds,
 * substitutions, and other operations.
 * @{
 */
/** @file
 * @brief Main entry for the logger C bindings
 *
 * Provides facilities to hook into Nix's activity/logging system via callbacks.
 */

#include "nix_api_util.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/**
 * @brief Called when an activity starts.
 *
 * @param[in] id Unique activity identifier (for tracking)
 * @param[in] description Human-readable description of the activity (e.g., "building /nix/store/...")
 * @param[in] activity_type String representation of activity type:
 *                            "realise", "build", "substitute", "copy-paths", etc.
 * @param[in] user_data Arbitrary user data passed to nix_set_logger_callbacks
 */
typedef void (*nix_activity_start_cb)(
    uint64_t id,
    const char * description,
    const char * activity_type,
    void * user_data);

/**
 * @brief Called when an activity finishes.
 *
 * @param[in] id Unique activity identifier (matches nix_activity_start_cb)
 * @param[in] user_data Arbitrary user data passed to nix_set_logger_callbacks
 */
typedef void (*nix_activity_stop_cb)(uint64_t id, void * user_data);

/**
 * @brief Called when an activity reports a result/progress.
 *
 * Called for any result type. The caller must interpret result_type to handle
 * the data appropriately.
 *
 * @param[in] activity_id Unique activity identifier (matches nix_activity_start_cb)
 * @param[in] result_type String representation of result type:
 *                         "progress", "build-log-line", "post-build-log-line", etc.
 * @param[in] field_count Number of fields in the result
 * @param[in] field_types Array of field types: 0 = int64, 1 = string
 * @param[in] int_values Array of int64 values (may contain data from string fields too)
 * @param[in] string_values Array of C string pointers (NULL for non-string fields)
 * @param[in] user_data Arbitrary user data passed to nix_set_logger_callbacks
 *
 * @note The caller must interpret field_count fields based on result_type.
 *       For example:
 *       - "progress": expects 4 int64 fields (done, expected, running, failed)
 *       - "build-log-line": expects 1 string field
 *       - "post-build-log-line": expects 1 string field
 */
typedef void (*nix_activity_result_cb)(
    uint64_t activity_id,
    const char * result_type,
    size_t field_count,
    const int * field_types,
    const int64_t * int_values,
    const char * const * string_values,
    void * user_data);

/**
 * @brief Register callbacks to observe Nix activities.
 *
 * This function must be called before any Nix operations that generate activities
 * (such as building, realizing strings with import-from-derivation, etc.).
 *
 * Only one set of callbacks can be active at a time. Calling this function again
 * replaces the previous callbacks.
 *
 * The caller is responsible for interpreting result_type and field data appropriately.
 * The C API passes raw data to maximum flexibility.
 *
 * @param[out] context Optional, stores error information
 * @param[in] on_start Callback invoked when an activity starts, or NULL to disable
 * @param[in] on_stop Callback invoked when an activity stops, or NULL to disable
 * @param[in] on_result Callback invoked for any activity result, or NULL to disable
 * @param[in] user_data Opaque pointer passed to all callbacks
 * @return NIX_OK if successful, error code otherwise
 *
 * @note The global logger is replaced when callbacks are set. This should happen
 *       during initialization before evaluation starts.
 *
 * Example:
 * @code{.c}
 * void on_start(uint64_t id, const char* desc, const char* type, void* data) {
 *     printf("[%s] %s\n", type, desc);
 * }
 *
 * void on_result(uint64_t id, const char* result_type, size_t field_count,
 *                const int* field_types, const int64_t* int_values,
 *                const char* const* string_values, void* data) {
 *     if (strcmp(result_type, "progress") == 0 && field_count >= 4) {
 *         printf("Progress: %ld/%ld\n", int_values[0], int_values[1]);
 *     }
 *     else if (strcmp(result_type, "build-log-line") == 0 && field_count >= 1) {
 *         printf("Log: %s\n", string_values[0]);
 *     }
 * }
 *
 * nix_c_context* ctx = nix_c_context_create();
 * nix_set_logger_callbacks(ctx, on_start, NULL, on_result, NULL);
 * @endcode
 */
nix_err nix_set_logger_callbacks(
    nix_c_context * context,
    nix_activity_start_cb on_start,
    nix_activity_stop_cb on_stop,
    nix_activity_result_cb on_result,
    void * user_data);

// cffi end
#ifdef __cplusplus
}
#endif

/** @} */
#endif // NIX_API_LOGGER_H
