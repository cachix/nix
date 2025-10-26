/**
 * @file
 * @brief Example C program using the BuildEnvironment C FFI API
 *
 * This example demonstrates how to use the nix_build_env API to:
 * 1. Parse a build environment from JSON (as produced by `nix print-dev-env --json`)
 * 2. Convert it to bash script format
 * 3. Convert it back to JSON
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nix_api_store.h"
#include "nix_api_build_env.h"
#include "nix_api_util.h"

/**
 * @brief Callback function to receive string chunks from the FFI
 *
 * This is called by the nix functions with chunks of the output.
 * In this example, we simply print them to stdout.
 */
static void print_string(const char * start, unsigned int n, void * user_data)
{
    (void) user_data;  // unused parameter
    fwrite(start, 1, n, stdout);
}

/**
 * @brief Example JSON representing a minimal build environment
 *
 * This is the format produced by `nix print-dev-env --json`.
 * In real usage, you would get this from that command or from a file.
 */
static const char * example_json = "{\n"
    "  \"variables\": {\n"
    "    \"PATH\": {\n"
    "      \"type\": \"exported\",\n"
    "      \"value\": \"/nix/store/...-python-3.11.0/bin:/nix/store/...-bash-5.2-p15/bin\"\n"
    "    },\n"
    "    \"PYTHONPATH\": {\n"
    "      \"type\": \"array\",\n"
    "      \"value\": [\n"
    "        \"/nix/store/.../lib/python3.11/site-packages\"\n"
    "      ]\n"
    "    },\n"
    "    \"MY_VAR\": {\n"
    "      \"type\": \"var\",\n"
    "      \"value\": \"hello\"\n"
    "    }\n"
    "  },\n"
    "  \"bashFunctions\": {\n"
    "    \"myFunction\": \"echo 'Hello from bash function'\"\n"
    "  }\n"
    "}\n";

int main(int argc, char * argv[])
{
    (void) argc;
    (void) argv;

    // Initialize the nix store library
    nix_c_context * context = nix_c_context_create();

    nix_err err = nix_libstore_init(context);
    if (err != NIX_OK) {
        fprintf(stderr, "Failed to initialize libstore\n");
        nix_c_context_free(context);
        return 1;
    }

    printf("=== BuildEnvironment C FFI Example ===\n\n");

    // Parse the JSON into a BuildEnvironment
    printf("1. Parsing JSON into BuildEnvironment...\n");
    nix_build_env * env = nix_build_env_parse_json(context, example_json);
    if (!env) {
        fprintf(stderr, "Failed to parse JSON: ");
        if (context && context->last_err) {
            fprintf(stderr, "%s\n", context->last_err->c_str());
        }
        nix_c_context_free(context);
        return 1;
    }
    printf("   Success!\n\n");

    // Convert to bash script
    printf("2. Converting to bash script format:\n");
    printf("   ---------- begin bash ----------\n");
    nix_build_env_to_bash(context, env, print_string, NULL);
    printf("   ----------- end bash -----------\n\n");

    // Check for structured attributes
    printf("3. Checking for structured attributes...\n");
    bool has_attrs = nix_build_env_has_structured_attrs(env);
    printf("   Has structured attributes: %s\n\n", has_attrs ? "yes" : "no");

    // Convert back to JSON
    printf("4. Converting back to JSON:\n");
    printf("   ---------- begin json ----------\n");
    nix_build_env_to_json(context, env, print_string, NULL);
    printf("   ----------- end json -----------\n\n");

    // Clean up
    nix_build_env_free(env);
    nix_c_context_free(context);

    printf("Example completed successfully!\n");
    return 0;
}
