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
    std::optional<nix::SourcePath> baseDirectory;
    // TODO: make an EvalSettings setting own this instead?
    bool readOnlyMode;
};

struct EvalState
{
    nix::fetchers::Settings fetchSettings;
    nix::EvalSettings settings;
    nix::EvalState state;

    EvalState(
        nix::fetchers::Settings && fs,
        nix::EvalSettings && s,
        const nix::LookupPath & lp,
        nix::ref<nix::Store> st,
        const std::optional<nix::SourcePath> & bd)
      : fetchSettings(std::move(fs)),
        settings(std::move(s)),
        state(lp, st, fetchSettings, settings)
    {
        if (bd) {
            state.baseDirectory = bd;
        }
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
