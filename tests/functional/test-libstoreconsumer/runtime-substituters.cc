#include "nix/store/globals.hh"
#include "nix/store/store-open.hh"
#include "nix/store/uds-remote-store.hh"
#include "nix/store/worker-protocol-impl.hh"
#include "nix/util/archive.hh"
#include "nix/util/signature/signer.hh"

#include <iostream>

using namespace nix;

struct TestRemoteStore : UDSRemoteStore
{
    explicit TestRemoteStore(ref<const UDSRemoteStoreConfig> config)
        : Store(*config)
        , LocalFSStore(*config)
        , RemoteStore(*config)
        , UDSRemoteStore(config)
    {
    }

    using RemoteStore::ConnectionHandle;
    using RemoteStore::getConnection;
};

int main(int argc, char ** argv)
{
    try {
        if (argc != 2)
            throw Error("expected a binary cache URI");

        initLibStore();
        settings.getWorkerSettings().substituters.override({});
        settings.trustedPublicKeys.override({});
        auto config = make_ref<UDSRemoteStoreConfig>(StoreConfig::Params{{"max-connections", "3"}});
        TestRemoteStore remote(config);
        Store & store = remote;
        auto cache = openStore(argv[1]);
        auto cacheUri = std::string(argv[1]) + "?priority=10";
        LocalSigner signer(SecretKey::generate("runtime-cache-test"));
        Strings keys{signer.getPublicKey().to_string()};
        unsigned int nextPath = 0;

        auto check = [&](TestRemoteStore::ConnectionHandle & conn, bool expected) {
            auto name = "runtime-cache-" + std::to_string(nextPath++);
            StringSink nar;
            dumpString(name, nar);
            auto hash = hashString(HashAlgorithm::SHA256, nar.s);
            ValidPathInfo info(cache->makeStorePath("output:out", hash, name), {*cache, hash});
            info.narSize = nar.s.size();
            info.sign(*cache, signer);
            StringSource source(nar.s);
            cache->addToStore(info, source, NoRepair, NoCheckSigs);

            bool succeeded = false;
            try {
                conn->to << WorkerProto::Op::EnsurePath;
                WorkerProto::write(store, *conn, info.path);
                conn.processStderr();
                readInt(conn->from);
                succeeded = true;
            } catch (Error &) {
                if (expected || !conn.daemonException)
                    throw;
            }
            if (succeeded != expected)
                throw Error("unexpected substitution result for '%s'", name);
        };

        auto checkPool = [&](bool expected) {
            auto first = remote.getConnection();
            auto second = remote.getConnection();
            auto third = remote.getConnection();
            check(first, expected);
            check(second, expected);
            check(third, expected);
        };

        store.addTrustedPublicKeys(keys);
        {
            auto first = remote.getConnection();
            auto second = remote.getConnection();
            check(first, false);
            check(second, false);
            if (!store.addSubstituter(cacheUri))
                throw Error("could not add substituter");
        }
        checkPool(true);

        if (!store.removeSubstituter(cacheUri))
            throw Error("could not remove substituter");
        checkPool(false);

        if (!store.addSubstituter(argv[1]))
            throw Error("could not re-add substituter");
        checkPool(true);

        store.removeTrustedPublicKeys(keys);
        checkPool(false);
        store.addTrustedPublicKeys(keys);
        checkPool(true);

        store.clearSubstituters();
        checkPool(false);
    } catch (const std::exception & e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
