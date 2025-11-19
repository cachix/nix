#include "nix/util/file-system.hh"
#include "nix_api_store.h"
#include "nix_api_util.h"
#include "nix_api_expr.h"
#include "nix_api_value.h"
#include "nix_api_flake.h"

#include "nix/expr/tests/nix_api_expr.hh"
#include "nix/util/tests/string_callback.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace nixC {

TEST_F(nix_api_store_test, nix_api_init_getFlake_exists)
{
    nix_libstore_init(ctx);
    assert_ctx_ok();
    nix_libexpr_init(ctx);
    assert_ctx_ok();

    auto settings = nix_flake_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, settings);

    nix_eval_state_builder * builder = nix_eval_state_builder_new(ctx, store);
    ASSERT_NE(nullptr, builder);
    assert_ctx_ok();

    nix_flake_settings_add_to_eval_state_builder(ctx, settings, builder);
    assert_ctx_ok();

    auto state = nix_eval_state_build(ctx, builder);
    assert_ctx_ok();
    ASSERT_NE(nullptr, state);

    nix_eval_state_builder_free(builder);

    auto value = nix_alloc_value(ctx, state);
    assert_ctx_ok();
    ASSERT_NE(nullptr, value);

    nix_err err = nix_expr_eval_from_string(ctx, state, "builtins.getFlake", ".", value);

    nix_state_free(state);

    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, err);
    ASSERT_EQ(NIX_TYPE_FUNCTION, nix_get_type(ctx, value));
}

TEST_F(nix_api_store_test, nix_api_flake_reference_not_absolute_no_basedir_fail)
{
    nix_libstore_init(ctx);
    assert_ctx_ok();
    nix_libexpr_init(ctx);
    assert_ctx_ok();

    auto settings = nix_flake_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, settings);

    auto fetchSettings = nix_fetchers_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, fetchSettings);

    auto parseFlags = nix_flake_reference_parse_flags_new(ctx, settings);

    std::string str(".#legacyPackages.aarch127-unknown...orion");
    std::string fragment;
    nix_flake_reference * flakeReference = nullptr;
    auto r = nix_flake_reference_and_fragment_from_string(
        ctx, fetchSettings, settings, parseFlags, str.data(), str.size(), &flakeReference, OBSERVE_STRING(fragment));

    ASSERT_NE(NIX_OK, r);
    ASSERT_EQ(nullptr, flakeReference);

    nix_flake_reference_parse_flags_free(parseFlags);
}

TEST_F(nix_api_store_test, nix_api_load_flake)
{
    auto tmpDir = nix::createTempDir();
    nix::AutoDelete delTmpDir(tmpDir, true);

    nix::writeFile(tmpDir + "/flake.nix", R"(
        {
            outputs = { ... }: {
                hello = "potato";
            };
        }
    )");

    nix_libstore_init(ctx);
    assert_ctx_ok();
    nix_libexpr_init(ctx);
    assert_ctx_ok();

    auto fetchSettings = nix_fetchers_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, fetchSettings);

    auto settings = nix_flake_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, settings);

    nix_eval_state_builder * builder = nix_eval_state_builder_new(ctx, store);
    ASSERT_NE(nullptr, builder);
    assert_ctx_ok();

    auto state = nix_eval_state_build(ctx, builder);
    assert_ctx_ok();
    ASSERT_NE(nullptr, state);

    nix_eval_state_builder_free(builder);

    auto parseFlags = nix_flake_reference_parse_flags_new(ctx, settings);
    assert_ctx_ok();
    ASSERT_NE(nullptr, parseFlags);

    auto r0 = nix_flake_reference_parse_flags_set_base_directory(ctx, parseFlags, tmpDir.c_str(), tmpDir.size());
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, r0);

    std::string fragment;
    const std::string ref = ".#legacyPackages.aarch127-unknown...orion";
    nix_flake_reference * flakeReference = nullptr;
    auto r = nix_flake_reference_and_fragment_from_string(
        ctx, fetchSettings, settings, parseFlags, ref.data(), ref.size(), &flakeReference, OBSERVE_STRING(fragment));
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, r);
    ASSERT_NE(nullptr, flakeReference);
    ASSERT_EQ(fragment, "legacyPackages.aarch127-unknown...orion");

    nix_flake_reference_parse_flags_free(parseFlags);

    auto lockFlags = nix_flake_lock_flags_new(ctx, settings);
    assert_ctx_ok();
    ASSERT_NE(nullptr, lockFlags);

    auto lockedFlake = nix_flake_lock(ctx, fetchSettings, settings, state, lockFlags, flakeReference);
    assert_ctx_ok();
    ASSERT_NE(nullptr, lockedFlake);

    nix_flake_lock_flags_free(lockFlags);

    auto value = nix_locked_flake_get_output_attrs(ctx, settings, state, lockedFlake);
    assert_ctx_ok();
    ASSERT_NE(nullptr, value);

    auto helloAttr = nix_get_attr_byname(ctx, value, state, "hello");
    assert_ctx_ok();
    ASSERT_NE(nullptr, helloAttr);

    std::string helloStr;
    nix_get_string(ctx, helloAttr, OBSERVE_STRING(helloStr));
    assert_ctx_ok();
    ASSERT_EQ("potato", helloStr);

    nix_value_decref(ctx, value);
    nix_locked_flake_free(lockedFlake);
    nix_flake_reference_free(flakeReference);
    nix_state_free(state);
    nix_flake_settings_free(settings);
}

TEST_F(nix_api_store_test, nix_api_load_flake_with_flags)
{
    nix_libstore_init(ctx);
    assert_ctx_ok();
    nix_libexpr_init(ctx);
    assert_ctx_ok();

    auto tmpDir = nix::createTempDir();
    nix::AutoDelete delTmpDir(tmpDir, true);

    nix::createDirs(tmpDir + "/b");
    nix::writeFile(tmpDir + "/b/flake.nix", R"(
        {
            outputs = { ... }: {
                hello = "BOB";
            };
        }
    )");

    nix::createDirs(tmpDir + "/a");
    nix::writeFile(tmpDir + "/a/flake.nix", R"(
        {
            inputs.b.url = ")" + tmpDir + R"(/b";
            outputs = { b, ... }: {
                hello = b.hello;
            };
        }
    )");

    nix::createDirs(tmpDir + "/c");
    nix::writeFile(tmpDir + "/c/flake.nix", R"(
        {
            outputs = { ... }: {
                hello = "Claire";
            };
        }
    )");

    nix_libstore_init(ctx);
    assert_ctx_ok();

    auto fetchSettings = nix_fetchers_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, fetchSettings);

    auto settings = nix_flake_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, settings);

    nix_eval_state_builder * builder = nix_eval_state_builder_new(ctx, store);
    ASSERT_NE(nullptr, builder);
    assert_ctx_ok();

    auto state = nix_eval_state_build(ctx, builder);
    assert_ctx_ok();
    ASSERT_NE(nullptr, state);

    nix_eval_state_builder_free(builder);

    auto parseFlags = nix_flake_reference_parse_flags_new(ctx, settings);
    assert_ctx_ok();
    ASSERT_NE(nullptr, parseFlags);

    auto r0 = nix_flake_reference_parse_flags_set_base_directory(ctx, parseFlags, tmpDir.c_str(), tmpDir.size());
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, r0);

    std::string fragment;
    const std::string ref = "./a";
    nix_flake_reference * flakeReference = nullptr;
    auto r = nix_flake_reference_and_fragment_from_string(
        ctx, fetchSettings, settings, parseFlags, ref.data(), ref.size(), &flakeReference, OBSERVE_STRING(fragment));
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, r);
    ASSERT_NE(nullptr, flakeReference);
    ASSERT_EQ(fragment, "");

    // Step 1: Do not update, fails

    auto lockFlags = nix_flake_lock_flags_new(ctx, settings);
    assert_ctx_ok();
    ASSERT_NE(nullptr, lockFlags);

    nix_flake_lock_flags_set_mode_check(ctx, lockFlags);
    assert_ctx_ok();

    // Step 2: Update but do not write, succeeds

    auto lockedFlake = nix_flake_lock(ctx, fetchSettings, settings, state, lockFlags, flakeReference);
    assert_ctx_err();
    ASSERT_EQ(nullptr, lockedFlake);

    nix_flake_lock_flags_set_mode_virtual(ctx, lockFlags);
    assert_ctx_ok();

    lockedFlake = nix_flake_lock(ctx, fetchSettings, settings, state, lockFlags, flakeReference);
    assert_ctx_ok();
    ASSERT_NE(nullptr, lockedFlake);

    // Get the output attrs
    auto value = nix_locked_flake_get_output_attrs(ctx, settings, state, lockedFlake);
    assert_ctx_ok();
    ASSERT_NE(nullptr, value);

    auto helloAttr = nix_get_attr_byname(ctx, value, state, "hello");
    assert_ctx_ok();
    ASSERT_NE(nullptr, helloAttr);

    std::string helloStr;
    nix_get_string(ctx, helloAttr, OBSERVE_STRING(helloStr));
    assert_ctx_ok();
    ASSERT_EQ("BOB", helloStr);

    nix_value_decref(ctx, value);
    nix_locked_flake_free(lockedFlake);

    // Step 3: Lock was not written, so Step 1 would fail again

    nix_flake_lock_flags_set_mode_check(ctx, lockFlags);

    lockedFlake = nix_flake_lock(ctx, fetchSettings, settings, state, lockFlags, flakeReference);
    assert_ctx_err();
    ASSERT_EQ(nullptr, lockedFlake);

    // Step 4: Update and write, succeeds

    nix_flake_lock_flags_set_mode_write_as_needed(ctx, lockFlags);
    assert_ctx_ok();

    lockedFlake = nix_flake_lock(ctx, fetchSettings, settings, state, lockFlags, flakeReference);
    assert_ctx_ok();
    ASSERT_NE(nullptr, lockedFlake);

    // Get the output attrs
    value = nix_locked_flake_get_output_attrs(ctx, settings, state, lockedFlake);
    assert_ctx_ok();
    ASSERT_NE(nullptr, value);

    helloAttr = nix_get_attr_byname(ctx, value, state, "hello");
    assert_ctx_ok();
    ASSERT_NE(nullptr, helloAttr);

    helloStr.clear();
    nix_get_string(ctx, helloAttr, OBSERVE_STRING(helloStr));
    assert_ctx_ok();
    ASSERT_EQ("BOB", helloStr);

    nix_value_decref(ctx, value);
    nix_locked_flake_free(lockedFlake);

    // Step 5: Lock was written, so Step 1 would succeed

    nix_flake_lock_flags_set_mode_check(ctx, lockFlags);
    assert_ctx_ok();

    lockedFlake = nix_flake_lock(ctx, fetchSettings, settings, state, lockFlags, flakeReference);
    assert_ctx_ok();
    ASSERT_NE(nullptr, lockedFlake);

    // Get the output attrs
    value = nix_locked_flake_get_output_attrs(ctx, settings, state, lockedFlake);
    assert_ctx_ok();
    ASSERT_NE(nullptr, value);

    helloAttr = nix_get_attr_byname(ctx, value, state, "hello");
    assert_ctx_ok();
    ASSERT_NE(nullptr, helloAttr);

    helloStr.clear();
    nix_get_string(ctx, helloAttr, OBSERVE_STRING(helloStr));
    assert_ctx_ok();
    ASSERT_EQ("BOB", helloStr);

    nix_value_decref(ctx, value);
    nix_locked_flake_free(lockedFlake);

    // Step 6: Lock with override, do not write

    nix_flake_lock_flags_set_mode_write_as_needed(ctx, lockFlags);
    assert_ctx_ok();

    nix_flake_reference * overrideFlakeReference = nullptr;
    nix_flake_reference_and_fragment_from_string(
        ctx, fetchSettings, settings, parseFlags, "./c", 3, &overrideFlakeReference, OBSERVE_STRING(fragment));
    assert_ctx_ok();
    ASSERT_NE(nullptr, overrideFlakeReference);

    nix_flake_lock_flags_add_input_override(ctx, lockFlags, "b", overrideFlakeReference);
    assert_ctx_ok();

    lockedFlake = nix_flake_lock(ctx, fetchSettings, settings, state, lockFlags, flakeReference);
    assert_ctx_ok();
    ASSERT_NE(nullptr, lockedFlake);

    // Get the output attrs
    value = nix_locked_flake_get_output_attrs(ctx, settings, state, lockedFlake);
    assert_ctx_ok();
    ASSERT_NE(nullptr, value);

    helloAttr = nix_get_attr_byname(ctx, value, state, "hello");
    assert_ctx_ok();
    ASSERT_NE(nullptr, helloAttr);

    helloStr.clear();
    nix_get_string(ctx, helloAttr, OBSERVE_STRING(helloStr));
    assert_ctx_ok();
    ASSERT_EQ("Claire", helloStr);

    nix_flake_reference_parse_flags_free(parseFlags);
    nix_flake_lock_flags_free(lockFlags);
    nix_flake_reference_free(flakeReference);
    nix_state_free(state);
    nix_flake_settings_free(settings);
}

TEST_F(nix_api_store_test, nix_flake_input_set_follows)
{
    nix_libstore_init(ctx);
    assert_ctx_ok();
    nix_libexpr_init(ctx);
    assert_ctx_ok();

    auto fetchSettings = nix_fetchers_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, fetchSettings);

    auto settings = nix_flake_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, settings);

    auto parseFlags = nix_flake_reference_parse_flags_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, parseFlags);

    nix_flake_reference * flakeRef = nullptr;
    std::string fragment;
    nix_flake_reference_and_fragment_from_string(
        ctx, fetchSettings, settings, parseFlags, "nixpkgs", 7, &flakeRef, OBSERVE_STRING(fragment));
    assert_ctx_ok();
    ASSERT_NE(nullptr, flakeRef);

    auto input = nix_flake_input_new(ctx, flakeRef, true);
    assert_ctx_ok();
    ASSERT_NE(nullptr, input);

    const char * followsPath = "dwarffs/nixpkgs";
    nix_err err = nix_flake_input_set_follows(ctx, input, followsPath, strlen(followsPath));
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, err);

    nix_flake_input_free(input);
    nix_flake_reference_free(flakeRef);
    nix_flake_reference_parse_flags_free(parseFlags);
    nix_flake_settings_free(settings);
    nix_fetchers_settings_free(fetchSettings);
}

TEST_F(nix_api_store_test, nix_flake_input_set_overrides)
{
    nix_libstore_init(ctx);
    assert_ctx_ok();
    nix_libexpr_init(ctx);
    assert_ctx_ok();

    auto fetchSettings = nix_fetchers_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, fetchSettings);

    auto settings = nix_flake_settings_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, settings);

    auto parseFlags = nix_flake_reference_parse_flags_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, parseFlags);

    // Create the main input (e.g., "foo" flake)
    nix_flake_reference * fooRef = nullptr;
    std::string fragment1;
    nix_flake_reference_and_fragment_from_string(
        ctx, fetchSettings, settings, parseFlags, "github:owner/foo", 18, &fooRef, OBSERVE_STRING(fragment1));
    assert_ctx_ok();
    ASSERT_NE(nullptr, fooRef);

    auto fooInput = nix_flake_input_new(ctx, fooRef, true);
    assert_ctx_ok();
    ASSERT_NE(nullptr, fooInput);

    // Create nested override: make foo's nixpkgs input follow our top-level nixpkgs
    auto overrides = nix_flake_inputs_new(ctx);
    assert_ctx_ok();
    ASSERT_NE(nullptr, overrides);

    nix_flake_reference * nixpkgsRef = nullptr;
    std::string fragment2;
    nix_flake_reference_and_fragment_from_string(
        ctx, fetchSettings, settings, parseFlags, "nixpkgs", 7, &nixpkgsRef, OBSERVE_STRING(fragment2));
    assert_ctx_ok();
    ASSERT_NE(nullptr, nixpkgsRef);

    auto nixpkgsOverride = nix_flake_input_new(ctx, nixpkgsRef, true);
    assert_ctx_ok();
    ASSERT_NE(nullptr, nixpkgsOverride);

    const char * followsPath = "nixpkgs";
    nix_err err = nix_flake_input_set_follows(ctx, nixpkgsOverride, followsPath, strlen(followsPath));
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, err);

    err = nix_flake_inputs_add(ctx, overrides, "nixpkgs", 7, nixpkgsOverride);
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, err);

    // Set the overrides on the foo input
    err = nix_flake_input_set_overrides(ctx, fooInput, overrides);
    assert_ctx_ok();
    ASSERT_EQ(NIX_OK, err);

    // Verify the overrides were set (they're now part of fooInput->input.overrides)
    ASSERT_EQ(1, fooInput->input.overrides.size());
    ASSERT_TRUE(fooInput->input.overrides.contains("nixpkgs"));

    nix_flake_input_free(fooInput);
    nix_flake_reference_free(fooRef);
    nix_flake_reference_free(nixpkgsRef);
    nix_flake_reference_parse_flags_free(parseFlags);
    nix_flake_settings_free(settings);
    nix_fetchers_settings_free(fetchSettings);
}

} // namespace nixC
