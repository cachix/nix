#include "nix/fetchers/fetch-to-store.hh"
#include "nix/fetchers/fetchers.hh"
#include "nix/fetchers/fetch-settings.hh"

namespace nix {

fetchers::Cache::Key makeSourcePathToHashCacheKey(
    const std::string & fingerprint,
    ContentAddressMethod method,
    const std::string & path)
{
    return fetchers::Cache::Key{"sourcePathToHash", {
        {"fingerprint", fingerprint},
        {"method", std::string{method.render()}},
        {"path", path}
    }};

}

StorePath fetchToStore(
    const fetchers::Settings & settings,
    Store & store,
    const SourcePath & path,
    FetchMode mode,
    std::string_view name,
    ContentAddressMethod method,
    PathFilter * filter,
    RepairFlag repair)
{

    return fetchToStore2(settings, store, path, mode, name, method, filter, repair).first;
}

std::pair<StorePath, Hash> fetchToStore2(
    const fetchers::Settings & settings,
    Store & store,
    const SourcePath & path,
    FetchMode mode,
    std::string_view name,
    ContentAddressMethod method,
    PathFilter * filter,
    RepairFlag repair)
{
    std::optional<fetchers::Cache::Key> cacheKey;

    if (!filter && path.accessor->fingerprint) {
        cacheKey = makeSourcePathToHashCacheKey(*path.accessor->fingerprint, method, path.path.abs());
        if (auto res = settings.getCache()->lookup(*cacheKey)) {
            debug("source path hash cache hit for '%s'", path);
            auto hash = Hash::parseSRI(fetchers::getStrAttr(*res, "hash"));
            auto storePath = store.makeFixedOutputPathFromCA(name,
                ContentAddressWithReferences::fromParts(method, hash, {}));
            if (store.isValidPath(storePath)) {
                debug("source path '%s' has valid store path '%s'", path, store.printStorePath(storePath));
                return {storePath, hash};
            }
            debug("source path '%s' not in store", path);
        }
    } else
        // FIXME: could still provide in-memory caching keyed on `SourcePath`.
        debug("source path '%s' is uncacheable", path);

    Activity act(*logger, lvlChatty, actUnknown,
        fmt(mode == FetchMode::DryRun ? "hashing '%s'" : "copying '%s' to the store", path));

    auto filter2 = filter ? *filter : defaultPathFilter;

    if (mode == FetchMode::DryRun) {
        auto [storePath, hash] = store.computeStorePath(
            name, path, method, HashAlgorithm::SHA256, {}, filter2);
        debug("hashed '%s' to '%s'", path, store.printStorePath(storePath));
        if (cacheKey)
            fetchers::getCache()->upsert(*cacheKey, {{"hash", hash.to_string(HashFormat::SRI, true)}});
        return {storePath, hash};
    } else {
        auto storePath = store.addToStore(
             name, path, method, HashAlgorithm::SHA256, {}, filter2, repair);

        debug("copied '%s' to '%s'", path, store.printStorePath(storePath));
        // FIXME: this is the wrong hash when method !=
        // ContentAddressMethod::Raw::NixArchive. Doesn't matter at
        // the moment since the only place where that's the case
        // doesn't use the hash.
        auto hash = store.queryPathInfo(storePath)->narHash;
        if (cacheKey)
            settings.getCache()->upsert(*cacheKey, {{"hash", hash.to_string(HashFormat::SRI, true)}});
        return {storePath, hash};
    }
}

}
