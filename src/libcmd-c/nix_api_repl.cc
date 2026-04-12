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
// and debug traces, then return immediately. This allows the caller to
// perform cleanup (e.g., restoring terminal state from a TUI) before
// running the REPL via nix_debugger_run_pending().
namespace {
    std::optional<nix::ValMap> g_captured_debug_env;
    std::optional<std::list<nix::DebugTrace>> g_captured_debug_traces;
    bool g_debug_pending = false;

    // Custom debugRepl that captures state and returns immediately
    nix::ReplExitStatus debugReplCapture(
        nix::ref<nix::EvalState> state,
        nix::ValMap const& env)
    {
        g_captured_debug_env = env;
        // Snapshot debug traces before stack unwinding destroys them.
        // DebugTraceStacker RAII guards pop entries from debugTraces
        // during C++ exception unwinding, so we must copy now.
        // Use emplace with iterator range since DebugTrace has reference
        // members and no copy assignment operator.
        g_captured_debug_traces.emplace(state->debugTraces.begin(), state->debugTraces.end());
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
        map->map[key] = value->value;
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
        nix::ReplExitStatus status = nix::AbstractNixRepl::runSimple(nix::ref<nix::EvalState>(state->ownedState), env);

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
        state->ownedSettings->ignoreExceptionsDuringTry.assign(true);
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

        // Restore debug traces that were captured before stack unwinding
        // cleared them. This makes :bt (backtrace) work in the REPL.
        if (g_captured_debug_traces) {
            state->state.debugTraces = std::move(*g_captured_debug_traces);
            g_captured_debug_traces.reset();
        }

        // Switch from the capture callback to the standard inline debugger.
        // This keeps debugRepl non-null so debug commands (:bt, :env, etc.)
        // remain available, while errors inside the REPL get handled
        // interactively instead of being captured again.
        state->state.debugRepl = &nix::AbstractNixRepl::runSimple;

        nix::ReplExitStatus status = nix::AbstractNixRepl::runSimple(
            nix::ref<nix::EvalState>(state->ownedState),
            env);

        if (exit_status != nullptr) {
            *exit_status = static_cast<nix_repl_exit_status>(status);
        }

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

} // extern "C"
