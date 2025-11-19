#ifndef NIX_API_FLAKE_H
#define NIX_API_FLAKE_H
/** @defgroup libflake libflake
 * @brief Bindings to the Nix Flakes library
 *
 * @{
 */
/** @file
 * @brief Main entry for the libflake C bindings
 */

#include "nix_api_fetchers.h"
#include "nix_api_store.h"
#include "nix_api_util.h"
#include "nix_api_expr.h"

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/**
 * @brief A settings object for configuring the behavior of the nix-flake-c library.
 * @see nix_flake_settings_new
 * @see nix_flake_settings_free
 */
typedef struct nix_flake_settings nix_flake_settings;

/**
 * @brief Context and parameters for parsing a flake reference
 * @see nix_flake_reference_parse_flags_free
 * @see nix_flake_reference_parse_string
 */
typedef struct nix_flake_reference_parse_flags nix_flake_reference_parse_flags;

/**
 * @brief A reference to a flake
 *
 * A flake reference specifies how to fetch a flake.
 *
 * @see nix_flake_reference_from_string
 * @see nix_flake_reference_free
 */
typedef struct nix_flake_reference nix_flake_reference;

/**
 * @brief Parameters for locking a flake
 * @see nix_flake_lock_flags_new
 * @see nix_flake_lock_flags_free
 * @see nix_flake_lock
 */
typedef struct nix_flake_lock_flags nix_flake_lock_flags;

/**
 * @brief A flake with a suitable lock (file or otherwise)
 * @see nix_flake_lock
 * @see nix_locked_flake_free
 * @see nix_locked_flake_get_output_attrs
 */
typedef struct nix_locked_flake nix_locked_flake;

/**
 * @brief A single flake input specification
 * @see nix_flake_input_new
 * @see nix_flake_input_free
 * @see nix_flake_inputs_add
 */
typedef struct nix_flake_input nix_flake_input;

/**
 * @brief A collection of flake inputs
 * @see nix_flake_inputs_new
 * @see nix_flake_inputs_free
 * @see nix_flake_inputs_add
 * @see nix_flake_lock_inputs
 */
typedef struct nix_flake_inputs nix_flake_inputs;

/**
 * @brief A flake lock file
 * @see nix_lock_file_new
 * @see nix_lock_file_free
 * @see nix_flake_lock_inputs
 * @see nix_lock_file_to_string
 */
typedef struct nix_lock_file nix_lock_file;

/**
 * @brief Iterator over inputs in a lock file
 * @see nix_lock_file_inputs_iterator_new
 * @see nix_lock_file_inputs_iterator_free
 */
typedef struct nix_lock_file_inputs_iterator nix_lock_file_inputs_iterator;
// Function prototypes
/**
 * Create a nix_flake_settings initialized with default values.
 * @param[out] context Optional, stores error information
 * @return A new nix_flake_settings or NULL on failure.
 * @see nix_flake_settings_free
 */
nix_flake_settings * nix_flake_settings_new(nix_c_context * context);

/**
 * @brief Release the resources associated with a nix_flake_settings.
 */
void nix_flake_settings_free(nix_flake_settings * settings);

/**
 * @brief Initialize a `nix_flake_settings` to contain `builtins.getFlake` and
 * potentially more.
 *
 * @warning This does not put the eval state in pure mode!
 *
 * @param[out] context Optional, stores error information
 * @param[in] settings The settings to use for e.g. `builtins.getFlake`
 * @param[in] builder The builder to modify
 */
nix_err nix_flake_settings_add_to_eval_state_builder(
    nix_c_context * context, nix_flake_settings * settings, nix_eval_state_builder * builder);

/**
 * @brief A new `nix_flake_reference_parse_flags` with defaults
 */
nix_flake_reference_parse_flags *
nix_flake_reference_parse_flags_new(nix_c_context * context, nix_flake_settings * settings);

/**
 * @brief Deallocate and release the resources associated with a `nix_flake_reference_parse_flags`.
 * Does not fail.
 * @param[in] flags the `nix_flake_reference_parse_flags *` to free
 */
void nix_flake_reference_parse_flags_free(nix_flake_reference_parse_flags * flags);

/**
 * @brief Provide a base directory for parsing relative flake references
 * @param[out] context Optional, stores error information
 * @param[in] flags The flags to modify
 * @param[in] baseDirectory The base directory to add
 * @param[in] baseDirectoryLen The length of baseDirectory
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_flake_reference_parse_flags_set_base_directory(
    nix_c_context * context,
    nix_flake_reference_parse_flags * flags,
    const char * baseDirectory,
    size_t baseDirectoryLen);

/**
 * @brief A new `nix_flake_lock_flags` with defaults
 * @param[in] settings Flake settings that may affect the defaults
 */
nix_flake_lock_flags * nix_flake_lock_flags_new(nix_c_context * context, nix_flake_settings * settings);

/**
 * @brief Deallocate and release the resources associated with a `nix_flake_lock_flags`.
 * Does not fail.
 * @param[in] settings the `nix_flake_lock_flags *` to free
 */
void nix_flake_lock_flags_free(nix_flake_lock_flags * settings);

/**
 * @brief Put the lock flags in a mode that checks whether the lock is up to date.
 * @param[out] context Optional, stores error information
 * @param[in] flags The flags to modify
 * @return NIX_OK on success, NIX_ERR on failure
 *
 * This causes `nix_flake_lock` to fail if the lock needs to be updated.
 */
nix_err nix_flake_lock_flags_set_mode_check(nix_c_context * context, nix_flake_lock_flags * flags);

/**
 * @brief Put the lock flags in a mode that updates the lock file in memory, if needed.
 * @param[out] context Optional, stores error information
 * @param[in] flags The flags to modify
 * @param[in] update Whether to allow updates
 *
 * This will cause `nix_flake_lock` to update the lock file in memory, if needed.
 */
nix_err nix_flake_lock_flags_set_mode_virtual(nix_c_context * context, nix_flake_lock_flags * flags);

/**
 * @brief Put the lock flags in a mode that updates the lock file on disk, if needed.
 * @param[out] context Optional, stores error information
 * @param[in] flags The flags to modify
 * @param[in] update Whether to allow updates
 *
 * This will cause `nix_flake_lock` to update the lock file on disk, if needed.
 */
nix_err nix_flake_lock_flags_set_mode_write_as_needed(nix_c_context * context, nix_flake_lock_flags * flags);

/**
 * @brief Add input overrides to the lock flags
 * @param[out] context Optional, stores error information
 * @param[in] flags The flags to modify
 * @param[in] inputPath The input path to override
 * @param[in] flakeRef The flake reference to use as the override
 *
 * This switches the `flags` to `nix_flake_lock_flags_set_mode_virtual` if not in mode
 * `nix_flake_lock_flags_set_mode_check`.
 */
nix_err nix_flake_lock_flags_add_input_override(
    nix_c_context * context, nix_flake_lock_flags * flags, const char * inputPath, nix_flake_reference * flakeRef);

/**
 * @brief Mark an input for updating in the lock flags
 * @param[out] context Optional, stores error information
 * @param[in] flags The flags to modify
 * @param[in] inputPath The input path to update (ignore existing lock)
 * @param[in] inputPathLen The length of inputPath
 * @return NIX_OK on success, NIX_ERR on failure
 *
 * When an input is added to the update set, any existing lock for that input
 * will be ignored, forcing it to be re-resolved. Other inputs will use their
 * existing locks.
 */
nix_err nix_flake_lock_flags_add_input_update(
    nix_c_context * context, nix_flake_lock_flags * flags, const char * inputPath, size_t inputPathLen);

/**
 * @brief Lock a flake, if not already locked.
 * @param[out] context Optional, stores error information
 * @param[in] settings The flake (and fetch) settings to use
 * @param[in] flags The locking flags to use
 * @param[in] flake The flake to lock
 */
nix_locked_flake * nix_flake_lock(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    nix_flake_settings * settings,
    EvalState * eval_state,
    nix_flake_lock_flags * flags,
    nix_flake_reference * flake);

/**
 * @brief Deallocate and release the resources associated with a `nix_locked_flake`.
 * Does not fail.
 * @param[in] locked_flake the `nix_locked_flake *` to free
 */
void nix_locked_flake_free(nix_locked_flake * locked_flake);

/**
 * @brief Parse a URL-like string into a `nix_flake_reference`.
 *
 * @param[out] context **context** – Optional, stores error information
 * @param[in] fetchSettings **context** – The fetch settings to use
 * @param[in] flakeSettings **context** – The flake settings to use
 * @param[in] parseFlags **context** – Specific context and parameters such as base directory
 *
 * @param[in] str **input** – The URI-like string to parse
 * @param[in] strLen **input** – The length of `str`
 *
 * @param[out] flakeReferenceOut **result** – The resulting flake reference
 * @param[in] fragmentCallback **result** – A callback to call with the fragment part of the URL
 * @param[in] fragmentCallbackUserData **result** – User data to pass to the fragment callback
 *
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_flake_reference_and_fragment_from_string(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    nix_flake_settings * flakeSettings,
    nix_flake_reference_parse_flags * parseFlags,
    const char * str,
    size_t strLen,
    nix_flake_reference ** flakeReferenceOut,
    nix_get_string_callback fragmentCallback,
    void * fragmentCallbackUserData);

/**
 * @brief Deallocate and release the resources associated with a `nix_flake_reference`.
 *
 * Does not fail.
 *
 * @param[in] store the `nix_flake_reference *` to free
 */
void nix_flake_reference_free(nix_flake_reference * store);

/**
 * @brief Get the output attributes of a flake.
 * @param[out] context Optional, stores error information
 * @param[in] settings The settings to use
 * @param[in] locked_flake the flake to get the output attributes from
 * @return A new nix_value or NULL on failure. Release the `nix_value` with `nix_value_decref`.
 */
nix_value * nix_locked_flake_get_output_attrs(
    nix_c_context * context, nix_flake_settings * settings, EvalState * evalState, nix_locked_flake * lockedFlake);

/**
 * @brief Create a new flake input from a flake reference
 * @param[out] context Optional, stores error information
 * @param[in] flakeRef The flake reference for this input
 * @param[in] isFlake Whether this input should be processed as a flake (true) or as static source (false)
 * @return A new nix_flake_input or NULL on failure
 * @see nix_flake_input_free
 */
nix_flake_input *
nix_flake_input_new(nix_c_context * context, nix_flake_reference * flakeRef, bool isFlake);

/**
 * @brief Set the follows attribute for a flake input
 * @param[out] context Optional, stores error information
 * @param[in] input The input to modify
 * @param[in] followsPath The input path to follow (e.g., "dwarffs/nixpkgs")
 * @param[in] followsPathLen The length of the followsPath string
 * @return NIX_OK on success, NIX_ERR on failure
 *
 * The follows attribute allows an input to reference another input's locked version
 * instead of having its own independent version. This is useful for deduplication.
 * After setting follows, the input should not have a ref set.
 */
nix_err nix_flake_input_set_follows(
    nix_c_context * context, nix_flake_input * input, const char * followsPath, size_t followsPathLen);

/**
 * @brief Set nested input overrides for a flake input
 * @param[out] context Optional, stores error information
 * @param[in] input The input to modify
 * @param[in] overrides The nested input overrides to apply (ownership is transferred)
 * @return NIX_OK on success, NIX_ERR on failure
 *
 * This allows you to override the inputs of a flake input. For example, if you have
 * a flake input "foo" and you want to override its "nixpkgs" input to follow your
 * top-level nixpkgs, you would:
 * 1. Create a nix_flake_inputs collection for the overrides
 * 2. Add a "nixpkgs" input with a follows attribute to it
 * 3. Call this function to set the overrides on the "foo" input
 *
 * After this call, the overrides collection is consumed and should not be freed or reused.
 */
nix_err nix_flake_input_set_overrides(
    nix_c_context * context, nix_flake_input * input, nix_flake_inputs * overrides);

/**
 * @brief Deallocate and release the resources associated with a nix_flake_input
 * Does not fail.
 * @param[in] input the nix_flake_input to free
 */
void nix_flake_input_free(nix_flake_input * input);

/**
 * @brief Create a new empty collection of flake inputs
 * @param[out] context Optional, stores error information
 * @return A new nix_flake_inputs or NULL on failure
 * @see nix_flake_inputs_free
 */
nix_flake_inputs * nix_flake_inputs_new(nix_c_context * context);

/**
 * @brief Add an input to the flake inputs collection
 * @param[out] context Optional, stores error information
 * @param[in] inputs The inputs collection to add to
 * @param[in] name The name/identifier for this input
 * @param[in] nameLen The length of the name string
 * @param[in] input The input to add (ownership is transferred)
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_flake_inputs_add(
    nix_c_context * context, nix_flake_inputs * inputs, const char * name, size_t nameLen, nix_flake_input * input);

/**
 * @brief Deallocate and release the resources associated with a nix_flake_inputs
 * Does not fail.
 * @param[in] inputs the nix_flake_inputs to free
 */
void nix_flake_inputs_free(nix_flake_inputs * inputs);

/**
 * @brief Create a new empty lock file
 * @param[out] context Optional, stores error information
 * @return A new nix_lock_file or NULL on failure
 * @see nix_lock_file_free
 */
nix_lock_file * nix_lock_file_new(nix_c_context * context);

/**
 * @brief Parse a lock file from a JSON string
 * @param[out] context Optional, stores error information
 * @param[in] fetchSettings The fetch settings to use for parsing
 * @param[in] content The JSON content to parse
 * @param[in] contentLen The length of the content string
 * @param[in] sourcePath Optional source path for error messages
 * @param[in] sourcePathLen Length of sourcePath (0 if NULL)
 * @return A new nix_lock_file or NULL on failure
 * @see nix_lock_file_free
 */
nix_lock_file * nix_lock_file_parse(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    const char * content,
    size_t contentLen,
    const char * sourcePath,
    size_t sourcePathLen);

/**
 * @brief Convert a lock file to a JSON string
 * @param[out] context Optional, stores error information
 * @param[in] lockFile The lock file to convert
 * @param[in] callback Called with the JSON string
 * @param[in] user_data optional, arbitrary data, passed to the callback when it's called
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_lock_file_to_string(
    nix_c_context * context, nix_lock_file * lockFile, nix_get_string_callback callback, void * user_data);

/**
 * @brief Deallocate and release the resources associated with a nix_lock_file
 * Does not fail.
 * @param[in] lockFile the nix_lock_file to free
 */
void nix_lock_file_free(nix_lock_file * lockFile);

/**
 * @brief Lock inputs without reading a top-level flake.nix
 *
 * This function takes manually-constructed flake inputs and computes
 * a lock file. EvalState is still required because transitive flake
 * inputs need to be fetched and evaluated.
 *
 * @param[out] context Optional, stores error information
 * @param[in] fetchSettings The fetch settings to use
 * @param[in] flakeSettings The flake settings to use
 * @param[in] evalState EvalState for fetching/evaluating transitive flakes
 * @param[in] inputs The inputs to lock (provided directly, not from flake.nix)
 * @param[in] sourcePath Base path for resolving relative input references
 * @param[in] sourcePathLen Length of sourcePath
 * @param[in] oldLockFile Existing lock file to use as basis (can be empty)
 * @param[in] flags Locking flags controlling update behavior
 * @return A new nix_lock_file with the computed locks or NULL on failure
 * @see nix_lock_file_free
 */
nix_lock_file * nix_flake_lock_inputs(
    nix_c_context * context,
    nix_fetchers_settings * fetchSettings,
    nix_flake_settings * flakeSettings,
    EvalState * evalState,
    nix_flake_inputs * inputs,
    const char * sourcePath,
    size_t sourcePathLen,
    nix_lock_file * oldLockFile,
    nix_flake_lock_flags * flags);

/**
 * @brief Compare two lock files for equality
 * @param[out] context Optional, stores error information
 * @param[in] lock_file_a The first lock file to compare
 * @param[in] lock_file_b The second lock file to compare
 * @param[out] are_equal Set to true if the lock files are equal, false otherwise
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_lock_file_equals(
    nix_c_context * context,
    nix_lock_file * lock_file_a,
    nix_lock_file * lock_file_b,
    bool * are_equal);

/**
 * @brief Generate a human-readable diff between two lock files
 *
 * Shows which inputs were added, removed, or updated between the old and new
 * lock files with colored ANSI output.
 *
 * @param[out] context Optional, stores error information
 * @param[in] old_lock_file The old lock file (baseline for comparison)
 * @param[in] new_lock_file The new lock file to compare against
 * @param[in] callback Called with the diff string
 * @param[in] user_data Optional data passed to the callback
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_lock_file_diff(
    nix_c_context * context,
    nix_lock_file * old_lock_file,
    nix_lock_file * new_lock_file,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Create a new iterator over all inputs in a lock file
 * @param[out] context Optional, stores error information
 * @param[in] lockFile The lock file to iterate over
 * @return A new iterator or NULL on failure
 * @see nix_lock_file_inputs_iterator_free
 */
nix_lock_file_inputs_iterator * nix_lock_file_inputs_iterator_new(
    nix_c_context * context,
    nix_lock_file * lockFile);

/**
 * @brief Check if the iterator has more inputs and advance to next
 * @param[in] iter The iterator to advance
 * @return true if iterator now points to a valid input, false if at end
 */
bool nix_lock_file_inputs_iterator_next(nix_lock_file_inputs_iterator * iter);

/**
 * @brief Get the attribute path of the current input (e.g., "nixpkgs" or "nix/nixpkgs")
 * @param[out] context Optional, stores error information
 * @param[in] iter The iterator
 * @param[in] callback Called with the attribute path string
 * @param[in] user_data Optional data passed to the callback
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_lock_file_inputs_iterator_get_attr_path(
    nix_c_context * context,
    nix_lock_file_inputs_iterator * iter,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Get the locked flake reference of the current input as a string
 *
 * For example: "github:NixOS/nixpkgs/6a08e6bb4e46ff7fcbb53d409b253f6bad8a28ce"
 *
 * @param[out] context Optional, stores error information
 * @param[in] iter The iterator
 * @param[in] callback Called with the locked reference string
 * @param[in] user_data Optional data passed to the callback
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_lock_file_inputs_iterator_get_locked_ref(
    nix_c_context * context,
    nix_lock_file_inputs_iterator * iter,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Get the original flake reference of the current input as a string
 *
 * @param[out] context Optional, stores error information
 * @param[in] iter The iterator
 * @param[in] callback Called with the original reference string
 * @param[in] user_data Optional data passed to the callback
 * @return NIX_OK on success, NIX_ERR on failure
 */
nix_err nix_lock_file_inputs_iterator_get_original_ref(
    nix_c_context * context,
    nix_lock_file_inputs_iterator * iter,
    nix_get_string_callback callback,
    void * user_data);

/**
 * @brief Free an iterator
 * Does not fail.
 * @param[in] iter The iterator to free
 */
void nix_lock_file_inputs_iterator_free(nix_lock_file_inputs_iterator * iter);
#ifdef __cplusplus
} // extern "C"
#endif

#endif
