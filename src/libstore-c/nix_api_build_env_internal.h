#ifndef NIX_API_BUILD_ENV_INTERNAL_H
#define NIX_API_BUILD_ENV_INTERNAL_H

#ifdef __cplusplus

#include "nix/store/build-environment.hh"
#include "nix/util/ref.hh"

/* These structures are not opaque on the C++ side */

struct nix_build_env
{
    nix::ref<nix::BuildEnvironment> env;
};

#endif

#endif
