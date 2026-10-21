#!/usr/bin/env bash

source common.sh

TODO_NixOS

sourceDir="$TEST_ROOT/source"
mkdir -p "$sourceDir"
echo 'from binary cache' > "$sourceDir/message"
touch -d '@1000000000' "$sourceDir" "$sourceDir/message"
tarball="$TEST_ROOT/source.tar.gz"
tar -czf "$tarball" -C "$TEST_ROOT" source

narHash=$(nix hash path "$sourceDir")
sourcePath=$(nix store add-path --name source "$sourceDir")
nix copy --to "file://$cacheDir" "$sourcePath"
nix-store --delete "$sourcePath"
expect 1 nix path-info "$sourcePath"

missingUrl="file://$TEST_ROOT/does-not-exist.tar.gz"
missingInput="{ type = \"tarball\"; url = \"$missingUrl\"; narHash = \"$narHash\"; }"

expect 1 nix eval --expr "builtins.fetchTree $missingInput"

result=$(nix eval --raw --substituters "file://$cacheDir" \
    --expr "builtins.readFile ((builtins.fetchFinalTree $missingInput).outPath + \"/message\")")
[[ "$result" = 'from binary cache' ]]
nix-store --check-validity "$sourcePath"

[[ $(nix eval --json --expr "builtins.attrNames (builtins.fetchFinalTree $missingInput)") = '["narHash","outPath"]' ]]
expect 1 nix eval --expr "builtins.fetchTree $missingInput"

metadataInput="{ type = \"tarball\"; url = \"$missingUrl\"; narHash = \"$narHash\"; lastModified = 1000000000; }"
[[ $(nix eval --raw --expr "(builtins.fetchFinalTree $metadataInput).lastModifiedDate") = 20010909014640 ]]

gitInput="{ type = \"git\"; url = \"file://$TEST_ROOT/does-not-exist.git\"; rev = \"0000000000000000000000000000000000000000\"; narHash = \"$narHash\"; shallow = false; }"
[[ $(nix eval --json --expr "builtins.attrNames (builtins.fetchFinalTree $gitInput)") = '["narHash","outPath","rev","shortRev","submodules"]' ]]

originInput="{ type = \"tarball\"; url = \"file://$tarball\"; narHash = \"$narHash\"; }"
nix-store --delete "$sourcePath"

[[ $(nix eval --json --expr "builtins.attrNames (builtins.fetchFinalTree $originInput)") = '["narHash","outPath"]' ]]
[[ $(nix eval --json --expr "builtins.attrNames (builtins.fetchTree $originInput)") = '["lastModified","lastModifiedDate","narHash","outPath"]' ]]
[[ $(nix eval --raw --expr "builtins.readFile ((builtins.fetchFinalTree $originInput).outPath + \"/message\")") = 'from binary cache' ]]

wrongHash=sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=
expect 1 nix eval --expr "builtins.fetchFinalTree { type = \"tarball\"; url = \"file://$tarball\"; narHash = \"$wrongHash\"; }"
expect 1 nix eval --expr "builtins.fetchFinalTree { type = \"tarball\"; url = \"$missingUrl\"; narHash = \"$wrongHash\"; }"

[[ $(nix eval --json --expr 'builtins ? fetchFinalTree' --extra-experimental-features '' --experimental-features nix-command) = false ]]
