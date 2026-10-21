---
synopsis: Expose builtins.fetchFinalTree to the user
issues: [16509]
prs: [14634]
---

Like `fetchTree`, but does not return any additional fetcher attributes (like `revCount`).
This allows inputs to be substituted if `narHash` is specified.
