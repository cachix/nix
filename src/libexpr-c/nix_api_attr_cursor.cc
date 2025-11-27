#include "nix_api_attr_cursor.h"
#include "nix_api_attr_cursor_internal.h"
#include "nix_api_expr.h"
#include "nix_api_value.h"
#include "nix_api_expr_internal.h"
#include "nix_api_util_internal.h"

#include "nix/util/hash.hh"

#include <mutex>
#include <unordered_map>

// Registry to hold shared_ptr ownership - prevents premature destruction
static std::unordered_map<nix_eval_cache *, std::shared_ptr<nix_eval_cache>> cache_registry;
static std::unordered_map<nix_attr_cursor *, std::shared_ptr<nix_attr_cursor>> cursor_registry;
static std::mutex registry_mutex;

nix_eval_cache * nix_eval_cache_create(
    nix_c_context * context,
    EvalState * state,
    nix_value * root_value,
    const char * cache_key)
{
    if (context) nix_clear_err(context);
    try {
        auto & nixState = state->state;
        auto * v = &reinterpret_cast<nix_value *>(root_value)->value;

        // Parse cache key as SHA256 hash if provided
        std::optional<nix::Hash> hash;
        if (cache_key) {
            hash = nix::Hash::parseAny(cache_key, nix::HashAlgorithm::SHA256);
        }

        // Create root loader that returns the provided value
        auto rootLoader = [v]() -> nix::Value * { return v; };

        // Create the eval cache
        auto cache = std::make_shared<nix::eval_cache::EvalCache>(
            hash ? std::optional{std::cref(*hash)} : std::nullopt,
            nixState,
            rootLoader);

        auto wrapper = std::make_shared<nix_eval_cache>(cache, nixState, v);

        auto * raw = wrapper.get();
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            cache_registry[raw] = wrapper;
        }
        return raw;
    } NIXC_CATCH_ERRS_NULL
}

void nix_eval_cache_free(nix_eval_cache * cache)
{
    if (!cache) return;
    std::lock_guard<std::mutex> lock(registry_mutex);
    cache_registry.erase(cache);
}

nix_attr_cursor * nix_eval_cache_get_root(
    nix_c_context * context,
    nix_eval_cache * cache)
{
    if (context) nix_clear_err(context);
    try {
        auto root = cache->cache->getRoot();

        auto wrapper = std::make_shared<nix_attr_cursor>();
        wrapper->cursor = root;
        wrapper->cache = cache;

        auto * raw = wrapper.get();
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            cursor_registry[raw] = wrapper;
        }
        return raw;
    } NIXC_CATCH_ERRS_NULL
}

void nix_attr_cursor_free(nix_attr_cursor * cursor)
{
    if (!cursor) return;
    std::lock_guard<std::mutex> lock(registry_mutex);
    cursor_registry.erase(cursor);
}

nix_attr_cursor * nix_attr_cursor_get_attr(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    const char * name)
{
    if (context) nix_clear_err(context);
    try {
        auto child = cursor->cursor->maybeGetAttr(name);
        if (!child) {
            return nullptr;  // Not found
        }

        auto wrapper = std::make_shared<nix_attr_cursor>();
        wrapper->cursor = child;
        wrapper->cache = cursor->cache;  // Propagate cache reference

        auto * raw = wrapper.get();
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            cursor_registry[raw] = wrapper;
        }
        return raw;
    } NIXC_CATCH_ERRS_NULL
}

int64_t nix_attr_cursor_get_attrs_count(nix_c_context * context, nix_attr_cursor * cursor)
{
    if (context) nix_clear_err(context);
    try {
        auto attrs = cursor->cursor->getAttrs();
        return static_cast<int64_t>(attrs.size());
    } NIXC_CATCH_ERRS_RES(-1)
}

nix_err nix_attr_cursor_get_attr_name(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    unsigned int index,
    nix_get_string_callback callback,
    void * user_data)
{
    if (context) nix_clear_err(context);
    try {
        auto attrs = cursor->cursor->getAttrs();
        if (index >= attrs.size()) {
            return nix_set_err_msg(context, NIX_ERR_KEY, "attribute index out of bounds");
        }

        // Resolve symbol to string using the EvalState's symbol table
        nix::Symbol sym = attrs[index];
        std::string_view name = cursor->cache->state.symbols[sym];

        callback(name.data(), name.size(), user_data);
        return NIX_OK;
    } NIXC_CATCH_ERRS
}

nix_err nix_attr_cursor_is_derivation(nix_c_context * context, nix_attr_cursor * cursor, bool * is_drv)
{
    if (context) nix_clear_err(context);
    try {
        *is_drv = cursor->cursor->isDerivation();
        return NIX_OK;
    } NIXC_CATCH_ERRS
}

nix_err nix_attr_cursor_get_string(
    nix_c_context * context,
    nix_attr_cursor * cursor,
    nix_get_string_callback callback,
    void * user_data)
{
    if (context) nix_clear_err(context);
    try {
        std::string str = cursor->cursor->getString();
        callback(str.c_str(), str.size(), user_data);
        return NIX_OK;
    } NIXC_CATCH_ERRS
}

nix_err nix_attr_cursor_get_bool(nix_c_context * context, nix_attr_cursor * cursor, bool * value)
{
    if (context) nix_clear_err(context);
    try {
        *value = cursor->cursor->getBool();
        return NIX_OK;
    } NIXC_CATCH_ERRS
}
