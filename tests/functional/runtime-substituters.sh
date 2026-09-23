#!/usr/bin/env bash

source common.sh

TODO_NixOS
clearStore
clearBinaryCache
startDaemon

"${_NIX_TEST_BUILD_DIR}/test-libstoreconsumer/runtime-substituters" "file://$cacheDir"
clearStore
clearBinaryCache
"${_NIX_TEST_BUILD_DIR}/test-libstoreconsumer/runtime-substituters" "file://$cacheDir" duplicate-refs
