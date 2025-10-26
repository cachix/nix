#!/usr/bin/env bash

source common.sh

clearStoreIfPossible

# Keep the derivation input-addressed while ca-derivations is enabled. This
# combination used to leave Nix's deferred output placeholder in `out`.
nix print-dev-env -f ../shell.nix shellDrv --json > "$TEST_ROOT/dev-env.json"

[[ $(jq -r '.variables.VAR_FROM_NIX.value' "$TEST_ROOT/dev-env.json") == bar ]]
