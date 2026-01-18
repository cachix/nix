#ifndef NIX_API_EXPR_INTERNAL_H
#define NIX_API_EXPR_INTERNAL_H

#include "nix/fetchers/fetch-settings.hh"
#include "nix/expr/eval.hh"
#include "nix/expr/eval-settings.hh"
#include "nix/expr/attr-set.hh"
#include "nix_api_value.h"
#include "nix/expr/search-path.hh"
#include "nix/util/source-path.hh"

extern "C" {

struct nix_eval_state_builder
{
    nix::ref<nix::Store> store;
    nix::EvalSettings settings;
    nix::fetchers::Settings fetchSettings;
    nix::LookupPath lookupPath;
    // TODO: make an EvalSettings setting own this instead?
    bool readOnlyMode;
};

struct EvalState
{
    nix::fetchers::Settings fetchSettings;
    nix::EvalSettings settings;
    // Use shared_ptr to enable shared_from_this() for debugger support
    std::shared_ptr<nix::EvalState> statePtr;
    nix::EvalState & state;

    EvalState(
        nix::fetchers::Settings && fs,
        nix::EvalSettings && s,
        const nix::LookupPath & lp,
        nix::ref<nix::Store> st)
      : fetchSettings(std::move(fs)),
        settings(std::move(s)),
        statePtr(std::make_shared<nix::EvalState>(lp, st, fetchSettings, settings)),
        state(*statePtr)
    {
    }
};

struct BindingsBuilder
{
    nix::BindingsBuilder builder;
};

struct ListBuilder
{
    nix::ListBuilder builder;
};

struct nix_value
{
    nix::Value value;
};

struct nix_string_return
{
    std::string str;
};

struct nix_printer
{
    std::ostream & s;
};

struct nix_string_context
{
    nix::NixStringContext & ctx;
};

struct nix_realised_string
{
    std::string str;
    std::vector<StorePath> storePaths;
};

} // extern "C"

#endif // NIX_API_EXPR_INTERNAL_H
