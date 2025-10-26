#include <cstring>
#include <sstream>

#include "nix_api_build_env.h"
#include "nix_api_build_env_internal.h"
#include "nix_api_store_internal.h"
#include "nix_api_util.h"
#include "nix_api_util_internal.h"

#include "nix/store/build-environment.hh"

extern "C" {

nix_build_env * nix_build_env_new(nix_c_context * context)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto env = nix::make_ref<nix::BuildEnvironment>();
        return new nix_build_env{env};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_build_env * nix_build_env_parse_json(nix_c_context * context, const char * json)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto env = nix::make_ref<nix::BuildEnvironment>(nix::BuildEnvironment::parseJSON(json));
        return new nix_build_env{env};
    }
    NIXC_CATCH_ERRS_NULL
}

void nix_build_env_free(nix_build_env * env)
{
    delete env;
}

nix_err nix_build_env_to_json(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto json = env->env->toJSON();
        auto jsonStr = json.dump();
        return call_nix_get_string_callback(jsonStr, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

nix_err nix_build_env_to_bash(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        std::ostringstream out;
        nix::StringSet ignoreVars;
        env->env->toBash(out, ignoreVars);
        auto bashStr = out.str();
        return call_nix_get_string_callback(bashStr, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

bool nix_build_env_has_structured_attrs(nix_build_env * env)
{
    return env->env->providesStructuredAttrs();
}

nix_err nix_build_env_get_attrs_json(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!env->env->providesStructuredAttrs()) {
            if (context)
                context->last_err_code = NIX_ERR_KEY;
            return NIX_ERR_KEY;
        }
        auto attrsJson = env->env->getAttrsJSON();
        return call_nix_get_string_callback(attrsJson, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

nix_err nix_build_env_get_attrs_sh(
    nix_c_context * context,
    nix_build_env * env,
    nix_get_string_callback callback,
    void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!env->env->providesStructuredAttrs()) {
            if (context)
                context->last_err_code = NIX_ERR_KEY;
            return NIX_ERR_KEY;
        }
        auto attrsSh = env->env->getAttrsSH();
        return call_nix_get_string_callback(attrsSh, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

nix_build_env * nix_build_env_from_derivation(
    nix_c_context * context,
    Store * store,
    const StorePath * drv_path)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        // Read the derivation from the store
        auto drv = store->ptr->readDerivation(drv_path->path);

        // Create a BuildEnvironment from the derivation using the new C++ method
        auto buildEnv = nix::make_ref<nix::BuildEnvironment>(
            nix::BuildEnvironment::fromDerivation(*store->ptr, drv)
        );

        return new nix_build_env{buildEnv};
    }
    NIXC_CATCH_ERRS_NULL
}

} // extern "C"
