#include "nix_api_store.h"
#include "nix_api_store_internal.h"
#include "nix_api_util.h"
#include "nix_api_util_internal.h"

#include "nix/store/path.hh"
#include "nix/store/store-api.hh"
#include "nix/store/store-open.hh"
#include "nix/store/build-result.hh"
#include "nix/store/local-fs-store.hh"
#include "nix/store/indirect-root-store.hh"
#include "nix/store/local-store.hh"
#include "nix/store/gc-store.hh"

#include "nix/store/globals.hh"

extern "C" {

nix_err nix_libstore_init(nix_c_context * context)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        nix::initLibStore();
    }
    NIXC_CATCH_ERRS
}

nix_err nix_libstore_init_no_load_config(nix_c_context * context)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        nix::initLibStore(false);
    }
    NIXC_CATCH_ERRS
}

Store * nix_store_open(nix_c_context * context, const char * uri, const char *** params)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        std::string uri_str = uri ? uri : "";

        if (uri_str.empty())
            return new Store{nix::openStore()};

        if (!params)
            return new Store{nix::openStore(uri_str)};

        nix::Store::Config::Params params_map;
        for (size_t i = 0; params[i] != nullptr; i++) {
            params_map[params[i][0]] = params[i][1];
        }
        return new Store{nix::openStore(uri_str, params_map)};
    }
    NIXC_CATCH_ERRS_NULL
}

void nix_store_free(Store * store)
{
    delete store;
}

nix_err nix_store_get_uri(nix_c_context * context, Store * store, nix_get_string_callback callback, void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto res = store->ptr->config.getReference().render(/*withParams=*/true);
        return call_nix_get_string_callback(res, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

nix_err
nix_store_get_storedir(nix_c_context * context, Store * store, nix_get_string_callback callback, void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        return call_nix_get_string_callback(store->ptr->storeDir, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

nix_err
nix_store_get_version(nix_c_context * context, Store * store, nix_get_string_callback callback, void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto res = store->ptr->getVersion();
        return call_nix_get_string_callback(res.value_or(""), callback, user_data);
    }
    NIXC_CATCH_ERRS
}

bool nix_store_is_valid_path(nix_c_context * context, Store * store, const StorePath * path)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        return store->ptr->isValidPath(path->path);
    }
    NIXC_CATCH_ERRS_RES(false);
}

nix_err nix_store_real_path(
    nix_c_context * context, Store * store, StorePath * path, nix_get_string_callback callback, void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto res = store->ptr->toRealPath(path->path);
        return call_nix_get_string_callback(res, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

StorePath * nix_store_parse_path(nix_c_context * context, Store * store, const char * path)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        nix::StorePath s = store->ptr->parseStorePath(path);
        return new StorePath{std::move(s)};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_err nix_store_get_fs_closure(
    nix_c_context * context,
    Store * store,
    const StorePath * store_path,
    bool flip_direction,
    bool include_outputs,
    bool include_derivers,
    void * userdata,
    void (*callback)(nix_c_context * context, void * userdata, const StorePath * store_path))
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        const auto nixStore = store->ptr;

        nix::StorePathSet set;
        nixStore->computeFSClosure(store_path->path, set, flip_direction, include_outputs, include_derivers);

        if (callback) {
            for (const auto & path : set) {
                const StorePath tmp{path};
                callback(context, userdata, &tmp);
                if (context && context->last_err_code != NIX_OK)
                    return context->last_err_code;
            }
        }
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_realise(
    nix_c_context * context,
    Store * store,
    StorePath * path,
    void * userdata,
    void (*callback)(void * userdata, const char *, const StorePath *))
{
    if (context)
        context->last_err_code = NIX_OK;
    try {

        const std::vector<nix::DerivedPath> paths{nix::DerivedPath::Built{
            .drvPath = nix::makeConstantStorePathRef(path->path), .outputs = nix::OutputsSpec::All{}}};

        const auto nixStore = store->ptr;
        auto results = nixStore->buildPathsWithResults(paths, nix::bmNormal, nixStore);

        assert(results.size() == 1);

        // Check if any builds failed
        for (auto & result : results) {
            if (auto * failureP = result.tryGetFailure())
                failureP->rethrow();
        }

        if (callback) {
            for (const auto & result : results) {
                if (auto * success = result.tryGetSuccess()) {
                    for (const auto & [outputName, realisation] : success->builtOutputs) {
                        StorePath p{realisation.outPath};
                        callback(userdata, outputName.c_str(), &p);
                    }
                }
            }
        }
    }
    NIXC_CATCH_ERRS
}

void nix_store_path_name(const StorePath * store_path, nix_get_string_callback callback, void * user_data)
{
    std::string_view name = store_path->path.name();
    callback(name.data(), name.size(), user_data);
}

void nix_store_path_free(StorePath * sp)
{
    delete sp;
}

void nix_derivation_free(nix_derivation * drv)
{
    delete drv;
}

StorePath * nix_store_path_clone(const StorePath * p)
{
    return new StorePath{p->path};
}

nix_derivation * nix_derivation_from_json(nix_c_context * context, Store * store, const char * json)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto drv = static_cast<nix::Derivation>(nlohmann::json::parse(json));

        auto drvPath = nix::writeDerivation(*store->ptr, drv, nix::NoRepair, /* read only */ true);

        drv.checkInvariants(*store->ptr, drvPath);

        return new nix_derivation{drv};
    }
    NIXC_CATCH_ERRS_NULL
}

StorePath * nix_add_derivation(nix_c_context * context, Store * store, nix_derivation * derivation)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        auto ret = nix::writeDerivation(*store->ptr, derivation->drv, nix::NoRepair);

        return new StorePath{ret};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_err nix_store_copy_closure(nix_c_context * context, Store * srcStore, Store * dstStore, StorePath * path)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        nix::RealisedPath::Set paths;
        paths.insert(path->path);
        nix::copyClosure(*srcStore->ptr, *dstStore->ptr, paths);
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_add_substituter(nix_c_context * context, Store * store, const char * uri)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store || !uri)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        bool success = store->ptr->addSubstituter(uri);
        return success ? NIX_OK : NIX_ERR_UNKNOWN;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_add_trusted_public_keys(
    nix_c_context * context,
    Store * store,
    const char ** keys,
    size_t n_keys)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        if (!keys || n_keys == 0)
            return NIX_OK; // Nothing to add

        // Convert C array to C++ Strings
        nix::Strings keysList;
        for (size_t i = 0; i < n_keys; ++i) {
            if (keys[i])
                keysList.push_back(keys[i]);
        }

        store->ptr->addTrustedPublicKeys(keysList);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_remove_trusted_public_keys(
    nix_c_context * context,
    Store * store,
    const char ** keys,
    size_t n_keys)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        if (!keys || n_keys == 0)
            return NIX_OK; // Nothing to remove

        // Convert C array to C++ Strings
        nix::Strings keysList;
        for (size_t i = 0; i < n_keys; ++i) {
            if (keys[i])
                keysList.push_back(keys[i]);
        }

        store->ptr->removeTrustedPublicKeys(keysList);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_remove_substituter(nix_c_context * context, Store * store, const char * uri)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store || !uri)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        bool success = store->ptr->removeSubstituter(uri);
        return success ? NIX_OK : NIX_ERR_KEY;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_list_substituters(
    nix_c_context * context, Store * store, nix_substituter_callback callback, void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store || !callback)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        auto subs = store->ptr->getSubstituters();
        for (const auto & sub : subs) {
            auto uri = sub->config.getHumanReadableURI();
            callback(uri.c_str(), sub->config.priority, user_data);
        }
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_clear_substituters(nix_c_context * context, Store * store)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        store->ptr->clearSubstituters();
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_add_perm_root(nix_c_context * context, Store * store, StorePath * path, const char * gc_root)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store || !path || !gc_root)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        auto localFSStore = dynamic_cast<nix::LocalFSStore *>(&*store->ptr);
        if (!localFSStore)
            throw nix::Unsupported("Store does not support permanent GC roots (not a LocalFSStore)");

        localFSStore->addPermRoot(path->path, gc_root);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_add_indirect_root(nix_c_context * context, Store * store, const char * symlink_path)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store || !symlink_path)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        auto indirectRootStore = dynamic_cast<nix::IndirectRootStore *>(&*store->ptr);
        if (!indirectRootStore)
            throw nix::Unsupported("Store does not support indirect GC roots (not an IndirectRootStore)");

        indirectRootStore->addIndirectRoot(symlink_path);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_delete_path(nix_c_context * context, Store * store, const char * path, uint64_t * bytes_freed)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store || !path)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        auto localStore = dynamic_cast<nix::LocalStore *>(&*store->ptr);
        if (!localStore)
            throw nix::Unsupported("Store does not support deleteStorePath (not a LocalStore)");

        uint64_t freed = 0;
        localStore->deleteStorePath(path, freed);

        if (bytes_freed)
            *bytes_freed = freed;

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_compute_fs_closure(
    nix_c_context * context,
    Store * store,
    StorePath ** paths,
    size_t num_paths,
    bool flip_direction,
    bool include_outputs,
    bool include_derivers,
    nix_store_path_callback callback,
    void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        if (num_paths == 0 || !paths) {
            // Empty input, nothing to do
            return NIX_OK;
        }

        // Convert StorePath** array to StorePathSet
        nix::StorePathSet startPaths;
        for (size_t i = 0; i < num_paths; i++) {
            if (!paths[i])
                return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;
            startPaths.insert(paths[i]->path);
        }

        // Compute the closure
        nix::StorePathSet closure;
        store->ptr->computeFSClosure(startPaths, closure, flip_direction, include_outputs, include_derivers);

        // Invoke callback for each path in the closure
        if (callback) {
            for (const auto & path : closure) {
                StorePath sp{path};
                callback(&sp, user_data);
            }
        }

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_store_collect_garbage(
    nix_c_context * context,
    Store * store,
    nix_gc_action action,
    StorePath ** paths_to_delete,
    size_t num_paths,
    bool ignore_liveness,
    uint64_t max_freed,
    nix_store_path_callback callback,
    void * user_data,
    uint64_t * bytes_freed)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store)
            return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;

        // Cast to GcStore
        auto gcStore = dynamic_cast<nix::GcStore *>(&*store->ptr);
        if (!gcStore)
            throw nix::Unsupported("Store does not support garbage collection");

        // Build GCOptions
        nix::GCOptions options;

        // Convert nix_gc_action to GCOptions::GCAction
        switch (action) {
            case NIX_GC_RETURN_LIVE:
                options.action = nix::GCOptions::gcReturnLive;
                break;
            case NIX_GC_RETURN_DEAD:
                options.action = nix::GCOptions::gcReturnDead;
                break;
            case NIX_GC_DELETE_DEAD:
                options.action = nix::GCOptions::gcDeleteDead;
                break;
            case NIX_GC_DELETE_SPECIFIC:
                options.action = nix::GCOptions::gcDeleteSpecific;
                break;
            default:
                return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;
        }

        options.ignoreLiveness = ignore_liveness;
        options.maxFreed = max_freed;

        // Add paths to delete if provided and action is DELETE_SPECIFIC
        if (action == NIX_GC_DELETE_SPECIFIC && paths_to_delete && num_paths > 0) {
            for (size_t i = 0; i < num_paths; i++) {
                if (!paths_to_delete[i])
                    return context ? context->last_err_code = NIX_ERR_KEY : NIX_ERR_KEY;
                options.pathsToDelete.insert(paths_to_delete[i]->path);
            }
        }

        // Run garbage collection
        nix::GCResults results;
        gcStore->collectGarbage(options, results);

        // Invoke callback for each path in the results
        if (callback) {
            for (const auto & pathStr : results.paths) {
                auto path = store->ptr->parseStorePath(pathStr);
                StorePath sp{path};
                callback(&sp, user_data);
            }
        }

        // Return bytes freed if requested
        if (bytes_freed)
            *bytes_freed = results.bytesFreed;

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_trusted_flag nix_store_is_trusted_client(nix_c_context * context, Store * store)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        if (!store)
            return NIX_TRUSTED_FLAG_UNKNOWN;

        auto result = store->ptr->isTrustedClient();
        if (!result.has_value())
            return NIX_TRUSTED_FLAG_UNKNOWN;
        return result.value() ? NIX_TRUSTED_FLAG_TRUSTED : NIX_TRUSTED_FLAG_NOT_TRUSTED;
    }
    NIXC_CATCH_ERRS_RES(NIX_TRUSTED_FLAG_UNKNOWN)
}

} // extern "C"
