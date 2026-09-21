#pragma once
///@file

#include "nix/expr/eval-gc.hh"
#include "nix/util/finally.hh"

#include <gtest/gtest.h>

#include <thread>

#if NIX_USE_BOEHMGC

namespace nix {

/**
 * Run `fn` on a separate thread registered with the Boehm GC.
 *
 * Useful for GC tests: pointers handled by `fn` never touch the calling
 * thread's stack or registers, so once the thread has exited the
 * conservative collector cannot retain them by accident.
 */
inline void runOnGCThread(auto fn)
{
    std::thread([&] {
        GC_stack_base base;
        ASSERT_EQ(GC_SUCCESS, GC_get_stack_base(&base));
        auto registered = GC_register_my_thread(&base);
        ASSERT_TRUE(registered == GC_SUCCESS || registered == GC_DUPLICATE);
        Finally unregister([&] {
            if (registered == GC_SUCCESS)
                GC_unregister_my_thread();
        });
        fn();
    }).join();
}

} // namespace nix

#endif
