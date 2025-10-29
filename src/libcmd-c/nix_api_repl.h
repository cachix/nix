#ifndef NIX_API_REPL_H
#define NIX_API_REPL_H
/** @defgroup libcmd libcmd
 * @brief Bindings to the Nix command utilities (REPL, etc.)
 *
 * @{
 */
/** @file
 * @brief Main entry for the libcmd C bindings
 */

#include "nix_api_expr.h"
#include "nix_api_store.h"
#include "nix_api_util.h"
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
 * @brief A map of string keys to Nix values for REPL environment
 */
typedef struct nix_valmap nix_valmap;

/**
 * @brief Exit status from the REPL
 */
typedef enum {
    /**
     * The REPL exited with `:quit`. The program should exit.
     */
    NIX_REPL_EXIT_QUIT_ALL = 0,
    /**
     * The REPL exited with `:continue`. The program should continue running.
     */
    NIX_REPL_EXIT_CONTINUE = 1,
} nix_repl_exit_status;

// Function prototypes

/**
 * @brief Initialize the Nix command library (REPL support).
 *
 * This function must be called at least once, at some point before using
 * any other libcmd-c functions. This function can be called multiple times,
 * and is idempotent.
 *
 * @param[out] context Optional, stores error information
 * @return NIX_OK if the initialization was successful, an error code otherwise.
 */
nix_err nix_libcmd_init(nix_c_context * context);

/**
 * @brief Create a new ValMap for collecting Nix values.
 *
 * ValMap is used to build a collection of name -> value mappings to inject
 * into the REPL environment's scope.
 *
 * @param[out] context Optional, stores error information
 * @return A new ValMap, or NULL on error
 * @see nix_valmap_free
 */
nix_valmap * nix_valmap_new(nix_c_context * context);

/**
 * @brief Free a ValMap and its contents.
 *
 * Does not fail.
 *
 * @param[in] map The ValMap to free. May be NULL.
 */
void nix_valmap_free(nix_valmap * map);

/**
 * @brief Insert a key-value pair into a ValMap.
 *
 * The value is copied/referenced but not owned by the map; you remain
 * responsible for its memory management.
 *
 * @param[out] context Optional, stores error information
 * @param[in] map The ValMap to insert into
 * @param[in] key The variable name (null-terminated string)
 * @param[in] value The Nix value to associate with the key
 * @return NIX_OK on success, an error code otherwise
 */
nix_err nix_valmap_insert(nix_c_context * context, nix_valmap * map, const char * key, nix_value * value);

/**
 * @brief Run a simple REPL with an EvalState and optional extra variables.
 *
 * This function launches an interactive Nix REPL with the given evaluation state
 * and optionally pre-populated variables from the ValMap. The REPL runs until the
 * user exits with `:quit` or `:continue`.
 *
 * @param[out] context Optional, stores error information
 * @param[in] state The evaluation state for the REPL
 * @param[in] extra_env Optional ValMap of additional variables to inject into scope.
 *            May be NULL to use an empty environment.
 * @param[out] exit_status The exit status from the REPL (if not NULL)
 * @return NIX_OK if the REPL completed successfully, an error code otherwise.
 *
 * @see nix_valmap_new, nix_valmap_insert, nix_repl_exit_status
 */
nix_err nix_repl_run_simple(
    nix_c_context * context,
    EvalState * state,
    nix_valmap * extra_env,
    nix_repl_exit_status * exit_status);

/**
 * @brief Enable the debugger for an evaluation state.
 *
 * When the debugger is enabled, any evaluation error will automatically enter
 * an interactive REPL where you can inspect the error context and variables.
 * This is equivalent to the `--debugger` CLI flag.
 *
 * @param[out] context Optional, stores error information
 * @param[in] state The evaluation state to enable the debugger for
 * @return NIX_OK on success, an error code otherwise
 *
 * @see nix_repl_run_simple
 */
nix_err nix_evalstate_enable_debugger(nix_c_context * context, EvalState * state);

// cffi end
#ifdef __cplusplus
}
#endif

/** @} */
#endif
