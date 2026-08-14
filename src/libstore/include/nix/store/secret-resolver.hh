#pragma once
///@file

#include "nix/util/types.hh"
#include "nix/util/ref.hh"

#include <chrono>
#include <compare>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace nix {

enum class SecretRepresentation {
    Inline,
    MaterialisedFile,
};

struct SecretPurpose
{
    std::string consumer;
    std::string operation;
    std::optional<std::string> host;
    std::optional<std::string> path;

    auto operator<=>(const SecretPurpose &) const = default;
};

struct SecretRequest
{
    std::string name;
    SecretRepresentation representation;
    SecretPurpose purpose;

    auto operator<=>(const SecretRequest &) const = default;
};

struct InlineSecret
{
    std::string value;
};

/**
 * A materialised secret file and the lease that keeps it alive.
 *
 * Implementations release the broker-side lease and remove any associated
 * materialisation from their destructor. Consumers must retain this object for
 * as long as they use `path()`; a bare path must never outlive the object.
 */
class SecretFile
{
public:
    virtual ~SecretFile() = default;

    virtual const std::filesystem::path & path() const noexcept = 0;
};

struct ResolvedSecret
{
    std::variant<InlineSecret, ref<SecretFile>> value;
    std::optional<std::chrono::system_clock::time_point> expiresAt;
};

/**
 * Resolve named secrets for one explicitly owned operation context.
 *
 * Implementations may keep instance-local transport or provider state, but
 * callers must not rely on a process-global resolver or cache.
 */
class SecretResolver
{
public:
    virtual ~SecretResolver() = default;

    virtual ResolvedSecret resolve(const SecretRequest & request) = 0;
};

} // namespace nix
