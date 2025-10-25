#ifndef NIX_API_REPL_INTERNAL_H
#define NIX_API_REPL_INTERNAL_H

#include "nix/expr/eval.hh"

extern "C" {

/**
 * @brief Internal representation of a ValMap
 *
 * This wraps nix::ValMap (which is std::map<std::string, Value*>)
 */
struct nix_valmap
{
    nix::ValMap map;
};

} // extern "C"

#endif
