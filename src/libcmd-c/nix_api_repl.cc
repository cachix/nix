#include <cstring>
#include <stdexcept>

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
        map->map[key] = &value->value;
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

        nix::ReplExitStatus status = nix::AbstractNixRepl::runSimple(nix::ref<nix::EvalState>(&state->state), env);

        if (exit_status != nullptr) {
            *exit_status = static_cast<nix_repl_exit_status>(status);
        }

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

} // extern "C"
