#include <set>
#include <string>

#include "nix_api_util.h"
#include "nix_api_store.h"
#include "nix_api_store/realisation.h"
#include "nix_api_store_internal.h"

#include "nix/store/dummy-store-impl.hh"
#include "nix/store/tests/nix_api_store.hh"
#include "nix/util/signature/local-keys.hh"

#include "store-tests-config.hh"

namespace nixC {

TEST_F(nix_api_store_test, nix_realisation_free_null_is_noop)
{
    ASSERT_NO_THROW(nix_realisation_free(nullptr));
}

TEST_F(nix_api_store_test, nix_store_query_realisation_invalid_id)
{
    nix_realisation * r = nix_store_query_realisation(ctx, store, "this-is-not-a-valid-drv-output-id");
    ASSERT_EQ(r, nullptr);
    ASSERT_NE(nix_err_code(ctx), NIX_OK) << "Malformed id should set an error";
}

TEST_F(nix_api_store_test, nix_store_query_realisation_missing_returns_null)
{
    // A syntactically valid but unknown id. The local-store fixture doesn't
    // have this realisation, so we expect (NULL, NIX_OK) — the documented
    // "missing" signal.
    auto id = nixStoreDir + "/g1w7hy3qg1w7hy3qg1w7hy3qg1w7hy3q-bar.drv^out";
    nix_realisation * r = nix_store_query_realisation(ctx, store, id.c_str());
    ASSERT_EQ(r, nullptr);
    ASSERT_EQ(nix_err_code(ctx), NIX_OK)
        << "Missing realisation must surface as NULL with NIX_OK; got error: " << nix_err_msg(nullptr, ctx, nullptr);
}

TEST_F(nix_api_util_context, nix_store_query_realisation_resolves_ca_input_derivation)
{
    nix_libstore_init(ctx);
    assert_ctx_ok();

    nix::EnableExperimentalFeature enableCA{"ca-derivations"};

    auto config = nix::make_ref<nix::DummyStoreConfig>(nix::DummyStoreConfig::Params{});
    config->readOnly = false;
    auto dummyStore = config->openDummyStore();
    Store cStore{dummyStore};

    auto caFloatingOutput = [] {
        return nix::DerivationOutput{nix::DerivationOutput::CAFloating{
            .method = nix::ContentAddressMethod::Raw::NixArchive,
            .hashAlgo = nix::HashAlgorithm::SHA256,
        }};
    };

    nix::Derivation depDrv;
    depDrv.name = "dep";
    depDrv.platform = nix::settings.thisSystem.get();
    depDrv.builder = "/bin/sh";
    depDrv.outputs = {{"out", caFloatingOutput()}};
    depDrv.env = {{"out", ""}};
    auto depDrvPath = dummyStore->writeDerivation(depDrv);

    auto depOutPath = nix::StorePath{nix::hashString(nix::HashAlgorithm::SHA1, "dep-output"), "dep-output"};
    dummyStore->registerDrvOutput(nix::Realisation{
        nix::UnkeyedRealisation{.outPath = depOutPath},
        nix::DrvOutput{depDrvPath, "out"},
    });

    nix::Derivation rootDrv;
    rootDrv.name = "root";
    rootDrv.platform = nix::settings.thisSystem.get();
    rootDrv.builder = "/bin/sh";
    rootDrv.outputs = {{"out", caFloatingOutput()}};
    rootDrv.env = {{"out", ""}};
    rootDrv.inputDrvs.map[depDrvPath].value.insert("out");
    auto rootDrvPath = dummyStore->writeDerivation(rootDrv);

    auto resolvedRootDrv = rootDrv.tryResolve(*dummyStore);
    ASSERT_TRUE(resolvedRootDrv);
    auto resolvedRootDrvPath = nix::computeStorePath(*dummyStore, nix::Derivation{*resolvedRootDrv});

    auto rootOutPath = nix::StorePath{nix::hashString(nix::HashAlgorithm::SHA1, "root-output"), "root-output"};
    dummyStore->registerDrvOutput(nix::Realisation{
        nix::UnkeyedRealisation{.outPath = rootOutPath},
        nix::DrvOutput{resolvedRootDrvPath, "out"},
    });

    ASSERT_FALSE(dummyStore->queryRealisation(nix::DrvOutput{rootDrvPath, "out"}));
    ASSERT_TRUE(dummyStore->queryRealisation(nix::DrvOutput{resolvedRootDrvPath, "out"}));

    auto rawId = nix::DrvOutput{rootDrvPath, "out"}.render(*dummyStore);
    nix_realisation * r = nix_store_query_realisation(ctx, &cStore, rawId.c_str());
    assert_ctx_ok();
    ASSERT_NE(r, nullptr);

    StorePath * outPath = nix_realisation_get_out_path(ctx, r);
    assert_ctx_ok();
    ASSERT_NE(outPath, nullptr);
    ASSERT_EQ(outPath->path, rootOutPath);

    nix_store_path_free(outPath);
    nix_realisation_free(r);
}

class NixApiRealisationTest : public NixApiStoreTestWithRealisedPath
{
public:
    std::string drvOutputId;

    void SetUp() override
    {
        NixApiStoreTestWithRealisedPath::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;

        auto ret = nix_derivation_get_outputs(
            ctx,
            store,
            drv,
            drvPath,
            &drvOutputId,
            +[](nix_c_context *, void * ud, const char * outputName, const char * id) {
                if (std::string_view{outputName} == "out")
                    *static_cast<std::string *>(ud) = id;
            });
        ASSERT_EQ(ret, NIX_OK);
        ASSERT_FALSE(drvOutputId.empty());
    }
};

TEST_F(NixApiRealisationTest, query_existing_realisation)
{
    nix_realisation * r = nix_store_query_realisation(ctx, store, drvOutputId.c_str());
    assert_ctx_ok();
    ASSERT_NE(r, nullptr) << "Realisation should exist after building the CA drv";

    StorePath * realisedOutPath = nix_realisation_get_out_path(ctx, r);
    assert_ctx_ok();
    ASSERT_NE(realisedOutPath, nullptr);
    ASSERT_TRUE(nix_store_is_valid_path(ctx, store, realisedOutPath))
        << "Realised out-path should be valid in the store";

    nix_store_path_free(realisedOutPath);
    nix_realisation_free(r);
}

TEST_F(NixApiRealisationTest, signatures_iteration)
{
    nix_realisation * r = nix_store_query_realisation(ctx, store, drvOutputId.c_str());
    ASSERT_NE(r, nullptr);

    // Locally-built realisation isn't expected to carry signatures, but
    // the iteration must succeed and (effectively) be a no-op.
    std::vector<std::string> sigs;
    nix_err ret = nix_realisation_get_signatures(
        ctx,
        r,
        &sigs,
        +[](nix_c_context *, void * ud, const char * sig) {
            static_cast<std::vector<std::string> *>(ud)->emplace_back(sig);
        });
    assert_ctx_ok();
    ASSERT_EQ(ret, NIX_OK);
    EXPECT_TRUE(sigs.empty()) << "Locally-built realisation should not carry signatures";

    nix_realisation_free(r);
}

TEST_F(NixApiRealisationTest, signatures_callback_error_propagates)
{
    nix_realisation * r = nix_store_query_realisation(ctx, store, drvOutputId.c_str());
    ASSERT_NE(r, nullptr);

    // Forcibly add a signature on the in-memory wrapper so iteration has
    // something to call the callback with.
    r->r.signatures.insert(nix::Signature{.keyName = "test-key", .sig = "fake-bytes"});

    int call_count = 0;
    nix_err ret = nix_realisation_get_signatures(
        ctx,
        r,
        &call_count,
        +[](nix_c_context * c, void * ud, const char *) {
            ++*static_cast<int *>(ud);
            nix_set_err_msg(c, NIX_ERR_UNKNOWN, "stop");
        });

    ASSERT_EQ(ret, NIX_ERR_UNKNOWN);
    ASSERT_EQ(call_count, 1) << "Iteration should stop after the first error";

    nix_realisation_free(r);
}

} // namespace nixC
