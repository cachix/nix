#pragma once
///@file

#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "nix/util/types.hh"
#include "nix/util/strings.hh"
#include "nix/store/store-api.hh"
#include "nix/store/derivations.hh"

namespace nix {

/**
 * @brief Represents a build environment extracted from a Nix derivation.
 *
 * This structure holds the environment variables, bash functions, and
 * structured attributes that make up a build environment. It can be
 * parsed from JSON (as produced by `get-env.sh`) and serialized back
 * to JSON or bash script format.
 */
struct BuildEnvironment
{
    /**
     * @brief Represents a simple string environment variable.
     */
    struct String
    {
        bool exported;  ///< Whether the variable should be exported
        std::string value;  ///< The variable's value

        bool operator==(const String & other) const
        {
            return exported == other.exported && value == other.value;
        }
    };

    /// Array-type environment variable
    using Array = std::vector<std::string>;

    /// Associative array (map) environment variable
    using Associative = StringMap;

    /// A value can be a string, array, or associative array
    using Value = std::variant<String, Array, Associative>;

    /// Map of environment variable names to their values
    std::map<std::string, Value> vars;
    /// Map of bash function names to their definitions
    StringMap bashFunctions;
    /// Optional structured attributes: (.attrs.json, .attrs.sh)
    std::optional<std::pair<std::string, std::string>> structuredAttrs;

    /**
     * @brief Parse a BuildEnvironment from a JSON object.
     *
     * The JSON structure should have the form:
     * ```json
     * {
     *   "variables": {
     *     "VAR_NAME": {"type": "var|exported|array|associative", "value": ...},
     *     ...
     *   },
     *   "bashFunctions": {
     *     "function_name": "function body...",
     *     ...
     *   },
     *   "structuredAttrs": {
     *     ".attrs.json": "...",
     *     ".attrs.sh": "..."
     *   }
     * }
     * ```
     */
    static BuildEnvironment fromJSON(const nlohmann::json & json);

    /**
     * @brief Parse a BuildEnvironment from a JSON string.
     *
     * @param in A JSON-formatted string
     * @return The parsed BuildEnvironment
     * @throws std::exception if JSON is invalid
     */
    static BuildEnvironment parseJSON(std::string_view in);

    /**
     * @brief Create a BuildEnvironment from a Derivation.
     *
     * Extracts environment variables and structured attributes from a derivation
     * to create a BuildEnvironment. This is useful for getting the build environment
     * that would be active when building a given derivation.
     *
     * @param store The Nix store (used for some store-specific operations)
     * @param drv The derivation to extract the environment from
     * @return A BuildEnvironment containing the variables and attributes from the derivation
     * @throws Error if extraction fails
     */
    static BuildEnvironment fromDerivation(const Store & store, const Derivation & drv);

    /**
     * @brief Convert the BuildEnvironment to JSON.
     *
     * @return A JSON object representing this environment
     */
    nlohmann::json toJSON() const;

    /**
     * @brief Check if this environment provides structured attributes.
     */
    bool providesStructuredAttrs() const
    {
        return structuredAttrs.has_value();
    }

    /**
     * @brief Get the structured attributes JSON content.
     *
     * @pre providesStructuredAttrs() must be true
     */
    std::string getAttrsJSON() const
    {
        return structuredAttrs->first;
    }

    /**
     * @brief Get the structured attributes shell script content.
     *
     * @pre providesStructuredAttrs() must be true
     */
    std::string getAttrsSH() const
    {
        return structuredAttrs->second;
    }

    /**
     * @brief Write the environment as bash shell code.
     *
     * Generates variable assignments, function definitions, and array declarations
     * in a format that can be sourced by bash.
     *
     * @param out Output stream to write bash code to
     * @param ignoreVars Set of variable names to skip
     */
    void toBash(std::ostream & out, const StringSet & ignoreVars) const;

    /**
     * @brief Extract a string value from a Value variant.
     *
     * @throws Error if the value is not a string type
     */
    static std::string getString(const Value & value);

    /**
     * @brief Extract an associative array from a Value variant.
     *
     * @throws Error if the value is not an associative array
     */
    static Associative getAssociative(const Value & value);

    /**
     * @brief Extract an array of strings from a Value variant.
     *
     * Can convert string values by tokenizing them, or extract array/associative
     * keys directly.
     *
     * @throws Error if the value cannot be converted to an array
     */
    static Array getStrings(const Value & value);

    bool operator==(const BuildEnvironment & other) const
    {
        return vars == other.vars && bashFunctions == other.bashFunctions;
    }

    /**
     * @brief Get the system architecture from the environment.
     *
     * Looks for the "system" variable in the environment, falling back to
     * the global settings value if not found.
     */
    std::string getSystem() const;
};

} // namespace nix
