#ifndef NIX_API_ATTR_CURSOR_INTERNAL_H
#define NIX_API_ATTR_CURSOR_INTERNAL_H

#include "nix/expr/eval.hh"
#include "nix/expr/eval-cache.hh"

#include <memory>

// Internal structures for nix_eval_cache and nix_attr_cursor
// Shared between nix_api_attr_cursor.cc and nix_api_search.cc

struct nix_eval_cache {
    std::shared_ptr<nix::eval_cache::EvalCache> cache;
    nix::EvalState & state;  // Reference to EvalState for symbol table access
    nix::Value * rootValue;  // Keep the root value alive

    nix_eval_cache(std::shared_ptr<nix::eval_cache::EvalCache> c, nix::EvalState & s, nix::Value * v)
        : cache(c), state(s), rootValue(v) {}
};

struct nix_attr_cursor {
    std::shared_ptr<nix::eval_cache::AttrCursor> cursor;
    nix_eval_cache * cache;  // Back-reference for symbol table access
};

#endif // NIX_API_ATTR_CURSOR_INTERNAL_H
