#include "nix/util/mounted-source-accessor.hh"

namespace nix {

struct MountedSourceAccessorImpl : MountedSourceAccessor
{
    std::map<CanonPath, ref<SourceAccessor>> mounts;
    // Track original paths for virtual store paths to enable proper path display
    std::map<CanonPath, std::optional<std::filesystem::path>> originalPaths;

    MountedSourceAccessorImpl(std::map<CanonPath, ref<SourceAccessor>> _mounts)
        : mounts(std::move(_mounts))
    {
        displayPrefix.clear();

        // Currently we require a root filesystem. This could be relaxed.
        assert(mounts.contains(CanonPath::root));

        // FIXME: return dummy parent directories automatically?
    }

    std::string readFile(const CanonPath & path) override
    {
        auto [accessor, subpath] = resolve(path);
        return accessor->readFile(subpath);
    }

    Stat lstat(const CanonPath & path) override
    {
        auto [accessor, subpath] = resolve(path);
        return accessor->lstat(subpath);
    }

    std::optional<Stat> maybeLstat(const CanonPath & path) override
    {
        auto [accessor, subpath] = resolve(path);
        return accessor->maybeLstat(subpath);
    }

    DirEntries readDirectory(const CanonPath & path) override
    {
        auto [accessor, subpath] = resolve(path);
        return accessor->readDirectory(subpath);
    }

    std::string readLink(const CanonPath & path) override
    {
        auto [accessor, subpath] = resolve(path);
        return accessor->readLink(subpath);
    }

    std::string showPath(const CanonPath & path) override
    {
        auto [accessor, subpath] = resolve(path);
        return displayPrefix + accessor->showPath(subpath) + displaySuffix;
    }

    std::pair<ref<SourceAccessor>, CanonPath> resolve(CanonPath path)
    {
        // Find the nearest parent of `path` that is a mount point.
        std::vector<std::string> subpath;
        while (true) {
            auto i = mounts.find(path);
            if (i != mounts.end()) {
                std::reverse(subpath.begin(), subpath.end());
                return {i->second, CanonPath(subpath)};
            }

            assert(!path.isRoot());
            subpath.push_back(std::string(*path.baseName()));
            path.pop();
        }
    }

    std::optional<std::filesystem::path> getPhysicalPath(const CanonPath & path) override
    {
        // First resolve which accessor and subpath we're dealing with
        auto [accessor, subpath] = resolve(path);

        // Get the mount point for this accessor
        for (const auto & [mountPoint, mountedAccessor] : mounts) {
            if (mountedAccessor == accessor) {
                // Check if we have an original path stored for this mount
                auto it = originalPaths.find(mountPoint);
                if (it != originalPaths.end() && it->second) {
                    // Reconstruct the full path using the original path
                    if (subpath.isRoot()) {
                        return *it->second;
                    } else {
                        return *it->second / subpath.rel();
                    }
                }
                break;
            }
        }

        // Fallback to the accessor's own physical path
        return accessor->getPhysicalPath(subpath);
    }

    void mount(CanonPath mountPoint, ref<SourceAccessor> accessor) override
    {
        // FIXME: thread-safety
        mounts.insert_or_assign(mountPoint, accessor);

        // Try to get the physical path from the accessor and store it
        auto physicalPath = accessor->getPhysicalPath(CanonPath::root);
        if (physicalPath) {
            originalPaths[mountPoint] = physicalPath;
        }
    }

    std::shared_ptr<SourceAccessor> getMount(CanonPath mountPoint) override
    {
        auto i = mounts.find(mountPoint);
        if (i != mounts.end())
            return i->second;
        else
            return nullptr;
    }
};

ref<MountedSourceAccessor> makeMountedSourceAccessor(std::map<CanonPath, ref<SourceAccessor>> mounts)
{
    return make_ref<MountedSourceAccessorImpl>(std::move(mounts));
}

}
