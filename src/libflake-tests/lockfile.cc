#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "nix/fetchers/attrs.hh"
#include "nix/fetchers/fetch-settings.hh"
#include "nix/flake/flakeref.hh"
#include "nix/flake/lockfile.hh"
#include "nix/util/ref.hh"

namespace nix::flake {

TEST(LockFile, DoesNotForceVolatileAttrsOfLocalInputs)
{
    fetchers::Settings fetchSettings;
    auto originalRef = parseFlakeRef(fetchSettings, "git+file:///local/repo");
    auto lockedRef = originalRef;

    unsigned int calls = 0;
    lockedRef.input.attrs.insert_or_assign(
        "revCount",
        fetchers::LazyAttr(
            make_ref<fetchers::LazyAttrComputation>(
                fetchers::LazyAttrComputation{.compute = [&calls]() -> fetchers::ResolvedAttr {
                    ++calls;
                    return uint64_t{42};
                }})));

    LockFile lockFile;
    lockFile.root->inputs.emplace("local", make_ref<LockedNode>(lockedRef, originalRef));

    auto json = lockFile.toJSON().first;

    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(json["nodes"]["local"]["locked"].contains("revCount"));
}

} // namespace nix::flake
