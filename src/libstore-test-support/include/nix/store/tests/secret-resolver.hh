#pragma once
///@file

#include "nix/store/secret-resolver.hh"

#include <functional>
#include <filesystem>
#include <utility>
#include <vector>

namespace nix::testing {

class CallbackSecretFile : public SecretFile
{
public:
    CallbackSecretFile(std::filesystem::path path, std::function<void()> onDestroy = {})
        : filePath(std::move(path))
        , onDestroy(std::move(onDestroy))
    {
    }

    ~CallbackSecretFile() override
    {
        if (onDestroy)
            onDestroy();
    }

    const std::filesystem::path & path() const noexcept override
    {
        return filePath;
    }

private:
    std::filesystem::path filePath;
    std::function<void()> onDestroy;
};

class CallbackSecretResolver : public SecretResolver
{
public:
    using Callback = std::function<ResolvedSecret(const SecretRequest &)>;

    explicit CallbackSecretResolver(Callback callback)
        : callback(std::move(callback))
    {
    }

    ResolvedSecret resolve(const SecretRequest & request) override
    {
        requests.push_back(request);
        return callback(request);
    }

    std::vector<SecretRequest> requests;

private:
    Callback callback;
};

} // namespace nix::testing
