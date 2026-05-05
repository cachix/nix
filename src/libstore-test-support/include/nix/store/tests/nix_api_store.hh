#pragma once
///@file
#include "nix/util/tests/nix_api_util.hh"

#include "nix/util/file-system.hh"
#include <filesystem>
#include <optional>

#include "nix_api_store.h"
#include "nix_api_store_internal.h"

#include "nix/store/globals.hh"
#include "nix/store/tests/libstore.hh"
#include "nix/util/util.hh"
#include "nix/util/tests/test-data.hh"

#include <filesystem>
#include <gtest/gtest.h>

namespace nixC {

class nix_api_store_test_base : public nix_api_util_context
{
public:
    nix_api_store_test_base()
    {
        nix_libstore_init(ctx);
    };

    ~nix_api_store_test_base() override
    try {
        nix::deletePath(nixDir);
    } catch (...) {
        nix::ignoreExceptionInDestructor();
    }

    std::string nixDir;
    std::string nixStoreDir;
    std::string nixStateDir;
    std::string nixLogDir;

protected:
    Store * open_local_store()
    {
#ifdef _WIN32
        // no `mkdtemp` with MinGW
        auto tmpl = nix::defaultTempDir() / "tests_nix-store.";
        for (size_t i = 0; true; ++i) {
            nixDir = tmpl.string() + std::to_string(i);
            if (std::filesystem::create_directory(nixDir))
                break;
        }
#else
        // resolve any symlinks in i.e. on macOS /tmp -> /private/tmp
        // because this is not allowed for a nix store.
        auto tmpl = nix::absPath(nix::defaultTempDir() / "tests_nix-store.XXXXXX", nullptr, true);
        nixDir = mkdtemp((char *) tmpl.c_str());
#endif

        nixStoreDir = nixDir + "/my_nix_store";
        nixStateDir = nixDir + "/my_state";
        nixLogDir = nixDir + "/my_log";

        // Options documented in `nix help-stores`
        const char * p1[] = {"store", nixStoreDir.c_str()};
        const char * p2[] = {"state", nixStateDir.c_str()};
        const char * p3[] = {"log", nixLogDir.c_str()};

        const char ** params[] = {p1, p2, p3, nullptr};

        auto * store = nix_store_open(ctx, "local", params);
        if (!store) {
            std::string errMsg = nix_err_msg(nullptr, ctx, nullptr);
            EXPECT_NE(store, nullptr) << "Could not open store: " << errMsg;
            assert(store);
        };
        return store;
    }
};

class nix_api_store_test : public nix_api_store_test_base
{
public:
    nix_api_store_test()
        : nix_api_store_test_base{} {};

    void SetUp() override
    {
#ifdef _WIN32
        GTEST_SKIP() << "Wine does not support symlinks needed for local store gcroots";
#endif
        store = open_local_store();
    }

    ~nix_api_store_test() override
    {
        if (store)
            nix_store_free(store);
    }

    Store * store = nullptr;
};

template<typename F>
struct LambdaAdapter
{
    F fun;

    template<typename... Args>
    static inline auto call(LambdaAdapter<F> * ths, Args... args)
    {
        return ths->fun(args...);
    }

    template<typename... Args>
    static auto call_void(void * ths, Args... args)
    {
        return call(static_cast<LambdaAdapter<F> *>(ths), args...);
    }
};

class NixApiStoreTestWithRealisedPath : public nix_api_store_test_base
{
public:
    std::optional<nix::EnableExperimentalFeature> enableCA;
    StorePath * drvPath = nullptr;
    nix_derivation * drv = nullptr;
    Store * store = nullptr;
    StorePath * outPath = nullptr;

    void SetUp() override
    {
        nix_api_store_test_base::SetUp();
#ifdef _WIN32
        GTEST_SKIP() << "Wine does not support symlinks needed for local store gcroots";
#endif

        enableCA.emplace("ca-derivations");
        nix::settings.getWorkerSettings().substituters = {};

        store = open_local_store();

        auto json = nix::readFile(nix::getUnitTestData() / "derivation/ca/self-contained.json");
        std::string jsonStr = nix::replaceStrings(json, "x86_64-linux", nix::settings.thisSystem.get());

        drv = nix_derivation_from_json(ctx, store, jsonStr.c_str());
        assert_ctx_ok();
        ASSERT_NE(drv, nullptr);

        drvPath = nix_add_derivation(ctx, store, drv);
        assert_ctx_ok();
        ASSERT_NE(drvPath, nullptr);

        auto cb = LambdaAdapter{.fun = [&](const char * outname, const StorePath * outPath_) {
            ASSERT_NE(outname, nullptr) << "Output name should not be NULL";
            auto is_valid_path = nix_store_is_valid_path(ctx, store, outPath_);
            ASSERT_EQ(is_valid_path, true);
            ASSERT_STREQ(outname, "out") << "Expected single 'out' output";
            ASSERT_EQ(outPath, nullptr) << "Output path callback should only be called once";
            outPath = nix_store_path_clone(outPath_);
        }};

        auto ret = nix_store_realise(
            ctx, store, drvPath, static_cast<void *>(&cb), decltype(cb)::call_void<const char *, const StorePath *>);
        assert_ctx_ok();
        ASSERT_EQ(ret, NIX_OK);
        ASSERT_NE(outPath, nullptr) << "Derivation should have produced an output";
    }

    void TearDown() override
    {
        if (drvPath)
            nix_store_path_free(drvPath);
        if (outPath)
            nix_store_path_free(outPath);
        if (drv)
            nix_derivation_free(drv);
        if (store)
            nix_store_free(store);

        nix_api_store_test_base::TearDown();
    }
};

} // namespace nixC
