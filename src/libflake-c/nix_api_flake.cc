#include <string>

#include "nix_api_flake.h"
#include "nix_api_flake_internal.hh"
#include "nix_api_util.h"
#include "nix_api_util_internal.h"
#include "nix_api_expr_internal.h"
#include "nix_api_fetchers_internal.hh"
#include "nix_api_fetchers.h"

#include "nix/flake/flake.hh"

extern "C" {

nix_flake_settings * nix_flake_settings_new(nix_c_context * context)
{
    nix_clear_err(context);
    try {
        auto settings = nix::make_ref<nix::flake::Settings>();
        return new nix_flake_settings{settings};
    }
    NIXC_CATCH_ERRS_NULL
}

void nix_flake_settings_free(nix_flake_settings * settings)
{
    delete settings;
}

nix_err nix_flake_settings_add_to_eval_state_builder(
    nix_c_context * context, nix_flake_settings * settings, nix_eval_state_builder * builder)
{
    nix_clear_err(context);
    try {
        settings->settings->configureEvalSettings(builder->settings);
    }
    NIXC_CATCH_ERRS
}

nix_flake_reference_parse_flags *
nix_flake_reference_parse_flags_new(nix_c_context * context, nix_flake_settings * settings)
{
    nix_clear_err(context);
    try {
        return new nix_flake_reference_parse_flags{
            .baseDirectory = std::nullopt,
        };
    }
    NIXC_CATCH_ERRS_NULL
}

void nix_flake_reference_parse_flags_free(nix_flake_reference_parse_flags * flags)
{
    delete flags;
}

nix_err nix_flake_reference_parse_flags_set_base_directory(
    nix_c_context * context,
    nix_flake_reference_parse_flags * flags,
    const char * baseDirectory,
    size_t baseDirectoryLen)
{
    nix_clear_err(context);
    try {
        flags->baseDirectory.emplace(nix::Path{std::string(baseDirectory, baseDirectoryLen)});
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_flake_reference_and_fragment_from_string(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    nix_flake_settings * flakeSettings,
    nix_flake_reference_parse_flags * parseFlags,
    const char * strData,
    size_t strSize,
    nix_flake_reference ** flakeReferenceOut,
    nix_get_string_callback fragmentCallback,
    void * fragmentCallbackUserData)
{
    nix_clear_err(context);
    *flakeReferenceOut = nullptr;
    try {
        std::string str(strData, strSize);

        auto [flakeRef, fragment] =
            nix::parseFlakeRefWithFragment(*fetchSettings->settings, str, parseFlags->baseDirectory, true);
        *flakeReferenceOut = new nix_flake_reference{nix::make_ref<nix::FlakeRef>(flakeRef)};
        return call_nix_get_string_callback(fragment, fragmentCallback, fragmentCallbackUserData);
    }
    NIXC_CATCH_ERRS
}

void nix_flake_reference_free(nix_flake_reference * flakeReference)
{
    delete flakeReference;
}

nix_flake_lock_flags * nix_flake_lock_flags_new(nix_c_context * context, nix_flake_settings * settings)
{
    nix_clear_err(context);
    try {
        auto lockSettings = nix::make_ref<nix::flake::LockFlags>(nix::flake::LockFlags{
            .recreateLockFile = false,
            .updateLockFile = true,  // == `nix_flake_lock_flags_set_mode_write_as_needed`
            .writeLockFile = true,   // == `nix_flake_lock_flags_set_mode_write_as_needed`
            .failOnUnlocked = false, // == `nix_flake_lock_flags_set_mode_write_as_needed`
            .useRegistries = false,
            .allowUnlocked = false, // == `nix_flake_lock_flags_set_mode_write_as_needed`
            .commitLockFile = false,

        });
        return new nix_flake_lock_flags{lockSettings};
    }
    NIXC_CATCH_ERRS_NULL
}

void nix_flake_lock_flags_free(nix_flake_lock_flags * flags)
{
    delete flags;
}

nix_err nix_flake_lock_flags_set_mode_virtual(nix_c_context * context, nix_flake_lock_flags * flags)
{
    nix_clear_err(context);
    try {
        flags->lockFlags->updateLockFile = true;
        flags->lockFlags->writeLockFile = false;
        flags->lockFlags->failOnUnlocked = false;
        flags->lockFlags->allowUnlocked = true;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_flake_lock_flags_set_mode_write_as_needed(nix_c_context * context, nix_flake_lock_flags * flags)
{
    nix_clear_err(context);
    try {
        flags->lockFlags->updateLockFile = true;
        flags->lockFlags->writeLockFile = true;
        flags->lockFlags->failOnUnlocked = false;
        flags->lockFlags->allowUnlocked = true;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_flake_lock_flags_set_mode_check(nix_c_context * context, nix_flake_lock_flags * flags)
{
    nix_clear_err(context);
    try {
        flags->lockFlags->updateLockFile = false;
        flags->lockFlags->writeLockFile = false;
        flags->lockFlags->failOnUnlocked = true;
        flags->lockFlags->allowUnlocked = false;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_flake_lock_flags_add_input_override(
    nix_c_context * context, nix_flake_lock_flags * flags, const char * inputPath, nix_flake_reference * flakeRef)
{
    nix_clear_err(context);
    try {
        auto path = nix::flake::parseInputAttrPath(inputPath);
        flags->lockFlags->inputOverrides.emplace(path, *flakeRef->flakeRef);
        if (flags->lockFlags->writeLockFile) {
            return nix_flake_lock_flags_set_mode_virtual(context, flags);
        }
    }
    NIXC_CATCH_ERRS
}

nix_err nix_flake_lock_flags_add_input_update(
    nix_c_context * context, nix_flake_lock_flags * flags, const char * inputPath, size_t inputPathLen)
{
    nix_clear_err(context);
    try {
        std::string pathStr(inputPath, inputPathLen);
        auto path = nix::flake::parseInputAttrPath(pathStr);
        flags->lockFlags->inputUpdates.insert(path);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_locked_flake * nix_flake_lock(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    nix_flake_settings * flakeSettings,
    EvalState * eval_state,
    nix_flake_lock_flags * flags,
    nix_flake_reference * flakeReference)
{
    nix_clear_err(context);
    try {
        eval_state->state.resetFileCache();
        auto lockedFlake = nix::make_ref<nix::flake::LockedFlake>(nix::flake::lockFlake(
            *flakeSettings->settings, eval_state->state, *flakeReference->flakeRef, *flags->lockFlags));
        return new nix_locked_flake{lockedFlake};
    }
    NIXC_CATCH_ERRS_NULL
}

void nix_locked_flake_free(nix_locked_flake * lockedFlake)
{
    delete lockedFlake;
}

nix_value * nix_locked_flake_get_output_attrs(
    nix_c_context * context, nix_flake_settings * settings, EvalState * evalState, nix_locked_flake * lockedFlake)
{
    nix_clear_err(context);
    try {
        auto v = nix_alloc_value(context, evalState);
        nix::flake::callFlake(evalState->state, *lockedFlake->lockedFlake, v->value);
        return v;
    }
    NIXC_CATCH_ERRS_NULL
}

nix_flake_input * nix_flake_input_new(nix_c_context * context, nix_flake_reference * flakeRef, bool isFlake)
{
    nix_clear_err(context);
    try {
        nix::flake::FlakeInput input;
        input.ref = *flakeRef->flakeRef;
        input.isFlake = isFlake;
        return new nix_flake_input{std::move(input)};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_err nix_flake_input_set_follows(
    nix_c_context * context, nix_flake_input * input, const char * followsPath, size_t followsPathLen)
{
    nix_clear_err(context);
    try {
        std::string pathStr(followsPath, followsPathLen);
        auto follows = nix::flake::parseInputAttrPath(pathStr);
        input->input.follows = follows;
        input->input.ref = std::nullopt;
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_flake_input_set_overrides(
    nix_c_context * context, nix_flake_input * input, nix_flake_inputs * overrides)
{
    nix_clear_err(context);
    try {
        input->input.overrides = std::move(overrides->inputs);
        delete overrides; // Transfer ownership
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

void nix_flake_input_free(nix_flake_input * input)
{
    delete input;
}

nix_flake_inputs * nix_flake_inputs_new(nix_c_context * context)
{
    nix_clear_err(context);
    try {
        return new nix_flake_inputs{nix::flake::FlakeInputs{}};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_err nix_flake_inputs_add(
    nix_c_context * context, nix_flake_inputs * inputs, const char * name, size_t nameLen, nix_flake_input * input)
{
    nix_clear_err(context);
    try {
        std::string inputName(name, nameLen);
        inputs->inputs.emplace(inputName, std::move(input->input));
        delete input; // Transfer ownership
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

void nix_flake_inputs_free(nix_flake_inputs * inputs)
{
    delete inputs;
}

nix_lock_file * nix_lock_file_new(nix_c_context * context)
{
    nix_clear_err(context);
    try {
        return new nix_lock_file{nix::flake::LockFile{}};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_lock_file * nix_lock_file_parse(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    const char * content,
    size_t contentLen,
    const char * sourcePath,
    size_t sourcePathLen)
{
    nix_clear_err(context);
    try {
        std::string contentStr(content, contentLen);
        std::string pathStr = sourcePathLen > 0 ? std::string(sourcePath, sourcePathLen) : "<string>";
        return new nix_lock_file{nix::flake::LockFile(*fetchSettings->settings, contentStr, pathStr)};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_err nix_lock_file_to_string(
    nix_c_context * context, nix_lock_file * lockFile, nix_get_string_callback callback, void * user_data)
{
    nix_clear_err(context);
    try {
        auto [jsonStr, keyMap] = lockFile->lockFile.to_string();
        return call_nix_get_string_callback(jsonStr, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

void nix_lock_file_free(nix_lock_file * lockFile)
{
    delete lockFile;
}

nix_lock_file * nix_flake_lock_inputs(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    nix_flake_settings * flakeSettings,
    EvalState * evalState,
    nix_flake_inputs * inputs,
    const char * sourcePath,
    size_t sourcePathLen,
    nix_lock_file * oldLockFile,
    nix_flake_lock_flags * flags)
{
    nix_clear_err(context);
    try {
        // Create source path
        std::string pathStr(sourcePath, sourcePathLen);
        nix::SourcePath srcPath = evalState->state.rootPath(nix::CanonPath(pathStr));

        // Create lock request
        nix::flake::InputLockRequest request{
            .inputs = inputs->inputs,
            .sourcePath = srcPath,
            .oldLockFile = oldLockFile ? &oldLockFile->lockFile : nullptr,
            .lockFlags = *flags->lockFlags,
        };

        // Call lockInputs
        auto [newLockFile, nodePaths] = nix::flake::lockInputs(*flakeSettings->settings, evalState->state, request);

        return new nix_lock_file{std::move(newLockFile)};
    }
    NIXC_CATCH_ERRS_NULL
}

nix_err nix_lock_file_equals(
    nix_c_context * context,
    nix_lock_file * lock_file_a,
    nix_lock_file * lock_file_b,
    bool * are_equal)
{
    nix_clear_err(context);
    try {
        *are_equal = (lock_file_a->lockFile == lock_file_b->lockFile);
        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

nix_err nix_lock_file_diff(
    nix_c_context * context,
    nix_lock_file * old_lock_file,
    nix_lock_file * new_lock_file,
    nix_get_string_callback callback,
    void * user_data)
{
    nix_clear_err(context);
    try {
        std::string diff = nix::flake::LockFile::diff(old_lock_file->lockFile, new_lock_file->lockFile);
        return call_nix_get_string_callback(diff, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

nix_lock_file_inputs_iterator * nix_lock_file_inputs_iterator_new(
    nix_c_context * context,
    nix_lock_file * lockFile)
{
    nix_clear_err(context);
    try {
        auto allInputs = lockFile->lockFile.getAllInputs();
        auto iter = new nix_lock_file_inputs_iterator{
            .allInputs = allInputs,
            .current = {},  // Will be set below
            .valid = !allInputs.empty(),
        };
        // Initialize current to point to the struct member's begin(), not the local variable's begin()
        iter->current = iter->allInputs.begin();
        return iter;
    }
    NIXC_CATCH_ERRS_NULL
}

bool nix_lock_file_inputs_iterator_next(nix_lock_file_inputs_iterator * iter)
{
    if (!iter->valid) {
        return false;
    }
    ++iter->current;
    iter->valid = (iter->current != iter->allInputs.end());
    return iter->valid;
}

nix_err nix_lock_file_inputs_iterator_get_attr_path(
    nix_c_context * context,
    nix_lock_file_inputs_iterator * iter,
    nix_get_string_callback callback,
    void * user_data)
{
    nix_clear_err(context);
    if (!iter->valid) {
        return nix_set_err_msg(context, NIX_ERR_UNKNOWN, "Iterator is not valid");
    }
    try {
        std::string attrPath = nix::flake::printInputAttrPath(iter->current->first);
        return call_nix_get_string_callback(attrPath, callback, user_data);
    }
    NIXC_CATCH_ERRS
}

nix_err nix_lock_file_inputs_iterator_get_locked_ref(
    nix_c_context * context,
    nix_lock_file_inputs_iterator * iter,
    nix_get_string_callback callback,
    void * user_data)
{
    nix_clear_err(context);
    if (!iter->valid) {
        return nix_set_err_msg(context, NIX_ERR_UNKNOWN, "Iterator is not valid");
    }
    try {
        const auto & edge = iter->current->second;

        // Edge is a variant of either LockedNode ref or InputAttrPath
        if (auto lockedNode = std::get_if<nix::ref<nix::flake::LockedNode>>(&edge)) {
            // Get the locked reference string
            std::string refStr = (*lockedNode)->lockedRef.to_string();
            return call_nix_get_string_callback(refStr, callback, user_data);
        } else {
            // It's an InputAttrPath (follows input), return empty or the path
            return call_nix_get_string_callback("", callback, user_data);
        }
    }
    NIXC_CATCH_ERRS
}

nix_err nix_lock_file_inputs_iterator_get_original_ref(
    nix_c_context * context,
    nix_lock_file_inputs_iterator * iter,
    nix_get_string_callback callback,
    void * user_data)
{
    nix_clear_err(context);
    if (!iter->valid) {
        return nix_set_err_msg(context, NIX_ERR_UNKNOWN, "Iterator is not valid");
    }
    try {
        const auto & edge = iter->current->second;

        // Edge is a variant of either LockedNode ref or InputAttrPath
        if (auto lockedNode = std::get_if<nix::ref<nix::flake::LockedNode>>(&edge)) {
            // Get the original reference string
            std::string refStr = (*lockedNode)->originalRef.to_string();
            return call_nix_get_string_callback(refStr, callback, user_data);
        } else {
            // It's an InputAttrPath (follows input), return empty
            return call_nix_get_string_callback("", callback, user_data);
        }
    }
    NIXC_CATCH_ERRS
}

void nix_lock_file_inputs_iterator_free(nix_lock_file_inputs_iterator * iter)
{
    delete iter;
}
} // extern "C"
