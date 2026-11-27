---
synopsis: Construct flake inputs without a reference in the C API
---

`nix_flake_input_new_empty` creates a flake input with neither a reference nor a follows target. Use it to set a follows target directly or to attach overrides to an existing nested input while keeping its source.
