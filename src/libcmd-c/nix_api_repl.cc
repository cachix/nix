#include <cstring>
#include <stdexcept>
#include <optional>

#include "nix/cmd/repl.hh"
#include "nix/expr/eval.hh"
#include "nix/store/store-open.hh"

#include "nix_api_repl.h"
#include "nix_api_repl_internal.h"
#include "nix_api_expr.h"
#include "nix_api_expr_internal.h"
#include "nix_api_store.h"
#include "nix_api_store_internal.h"
#include "nix_api_util.h"
#include "nix_api_util_internal.h"

// Captured debug state for deferred REPL execution
// When debugger is enabled and an error occurs, we capture the environment
// and return immediately, allowing the caller to run the REPL later.
namespace {
    std::optional<nix::ValMap> g_captured_debug_env;
    bool g_debug_pending = false;

    // Custom debugRepl that captures state and returns immediately
    nix::ReplExitStatus debugReplCapture(
        nix::ref<nix::EvalState> state,
        nix::ValMap const& env)
    {
        g_captured_debug_env = env;
        g_debug_pending = true;
        return nix::ReplExitStatus::QuitAll;
    }
}

extern "C" {

nix_err nix_libcmd_init(nix_c_context * context)
{
    if (context)
        context->last_err_code = NIX_OK;
    auto ret = nix_libutil_init(context);
    if (ret != NIX_OK)
        return ret;
    ret = nix_libstore_init(context);
    if (ret != NIX_OK)
        return ret;
    ret = nix_libexpr_init(context);
    if (ret != NIX_OK)
        return ret;
    return NIX_OK;
}

nix_valmap * nix_valmap_new(nix_c_context * context)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto * map = new nix_valmap();
        return map;
    }
    NIXC_CATCH_ERRS_RES(nullptr)
}

void nix_valmap_free(nix_valmap * map)
{
    if (map == nullptr)
        return;
    delete map;
}

nix_err nix_valmap_insert(nix_c_context * context, nix_valmap * map, const char * key, nix_value * value)
{
    if (context)
        context->last_err_code = NIX_OK;
    if (map == nullptr || key == nullptr || value == nullptr) {
        return NIX_OK;
    }
    try {
        // NOTE: The FFI layer passes a raw nix::Value* pointer cast as nix_value*
        // We interpret it as a raw pointer and store it directly
        auto * raw_value = reinterpret_cast<nix::Value *>(value);
        map->map[key] = raw_value;
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_repl_run_simple(
    nix_c_context * context,
    EvalState * state,
    nix_valmap * extra_env,
    nix_repl_exit_status * exit_status)
{
    if (context)
        context->last_err_code = NIX_OK;
    if (state == nullptr) {
        return NIX_OK;
    }
    try {
        nix::ValMap env;
        if (extra_env != nullptr) {
            env = extra_env->map;
        }

        // Use the shared_ptr stored in the EvalState struct
        nix::ReplExitStatus status = nix::AbstractNixRepl::runSimple(nix::ref<nix::EvalState>(state->statePtr), env);

        if (exit_status != nullptr) {
            *exit_status = static_cast<nix_repl_exit_status>(status);
        }

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_evalstate_enable_debugger(nix_c_context * context, EvalState * state)
{
    if (context)
        context->last_err_code = NIX_OK;
    if (state == nullptr) {
        return NIX_OK;
    }
    try {
        // Use capture callback - captures debug env and returns immediately
        // Caller should check nix_debugger_is_pending() and call nix_debugger_run_pending()
        state->state.debugRepl = &debugReplCapture;
        // By default, ignore exceptions inside tryEval when debugger is enabled
        // Otherwise the debugger breaks on expected/handled errors
        state->settings.ignoreExceptionsDuringTry.assign(true);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

bool nix_debugger_is_pending()
{
    return g_debug_pending;
}

nix_err nix_debugger_run_pending(
    nix_c_context * context,
    EvalState * state,
    nix_repl_exit_status * exit_status)
{
    if (context)
        context->last_err_code = NIX_OK;
    if (!g_debug_pending || !g_captured_debug_env || state == nullptr) {
        return NIX_OK;
    }
    try {
        g_debug_pending = false;
        auto env = std::move(*g_captured_debug_env);
        g_captured_debug_env.reset();

        // Clear the debugRepl callback before running the REPL
        // so errors inside the REPL show proper tracebacks instead of
        // being captured again by debugReplCapture
        state->state.debugRepl = nullptr;

        nix::ReplExitStatus status = nix::AbstractNixRepl::runSimple(
            nix::ref<nix::EvalState>(state->statePtr),
            env);

        if (exit_status != nullptr) {
            *exit_status = static_cast<nix_repl_exit_status>(status);
        }

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

} // extern "C"
