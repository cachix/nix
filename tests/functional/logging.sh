#!/usr/bin/env bash

source common.sh

TODO_NixOS

clearStore

path=$(nix-build dependencies.nix --no-out-link)

# Test nix-store -l.
[ "$(nix-store -l "$path")" = FOO ]

# Test compressed logs.
clearStore
rm -rf "$NIX_LOG_DIR"
(! nix-store -l "$path")
nix-build dependencies.nix --no-out-link --compress-build-log
[ "$(nix-store -l "$path")" = FOO ]

# test whether empty logs work fine with `nix log`.
builder="$(realpath "$(mktemp)")"
echo -e "#!/bin/sh\nmkdir \$out" > "$builder"
outp="$(nix-build -E \
    'with import '"${config_nix}"'; mkDerivation { name = "fnord"; builder = '"$builder"'; }' \
    --out-link "$(mktemp -d)/result")"

test -d "$outp"

nix log "$outp"

if isDaemonNewer "2.26"; then
    # Build works despite ill-formed structured build log entries.
    expectStderr 0 nix build -f ./logging/unusual-logging.nix --no-link | grepQuiet 'warning: Unable to handle a JSON message from the derivation builder:'
fi

# Test json-log-path.
if [[ "$NIX_REMOTE" != "daemon" ]]; then
    clearStore
    nix build -vv --file dependencies.nix --no-link --json-log-path "$TEST_ROOT/log.json" 2>&1 | grepQuiet 'building.*dependencies-top.drv'
    jq < "$TEST_ROOT/log.json"
    grep '{"action":"start","fields":\[".*-dependencies-top.drv","",1,1\],"id":.*,"level":3,"parent":0' "$TEST_ROOT/log.json" >&2
    (( $(grep -c '{"action":"msg","level":5,"msg":"executing builder .*"}' "$TEST_ROOT/log.json" ) == 5 ))

    # Test that error messages include parent field linking to failed build activity.
    clearStore
    rm -f "$TEST_ROOT/fail-log.json"
    expect 1 nix build --impure --file ./logging/failing-build.nix --no-link --json-log-path "$TEST_ROOT/fail-log.json" 2>&1

    # Find the build activity ID (type 105 = actBuild)
    build_activity_id=$(jq -r 'select(.action == "start" and .type == 105) | .id' "$TEST_ROOT/fail-log.json" | tail -1)
    [[ -n "$build_activity_id" ]] || { echo "Could not find build activity"; exit 1; }

    # Verify at least one error message has parent field pointing to build activity
    # (There may be multiple error messages; we check if any has the correct parent)
    error_with_parent=$(jq -r 'select(.action == "msg" and .level == 0 and (.msg | contains("Cannot build")) and .parent) | .parent' "$TEST_ROOT/fail-log.json" | head -1)
    [[ "$error_with_parent" == "$build_activity_id" ]] || { echo "No error message with parent ($error_with_parent) matching build activity ($build_activity_id)"; exit 1; }
    echo "Error message correctly links to build activity $build_activity_id"
fi
