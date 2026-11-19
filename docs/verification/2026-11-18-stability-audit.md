# Stability audit — November 18, 2026

The audit covered prediction and movement lifetimes, command handling, input,
configuration and skin persistence, text rendering, and the existing animation,
lag-record, resource-cleanup and shutdown paths. The final review revisited the
changed callers and checked project registration and snapshot consistency.

## Fixed failures

| Trigger | Corrected behavior | Verification |
| --- | --- | --- |
| Prediction cannot start because an interface or command is unavailable | Cleanup leaves the engine clock alone; repeated cleanup is harmless | Two prediction lifecycle tests |
| Map shutdown retains the previous prediction command history | Shutdown resets prediction history and flags | Shutdown call-path review and DLL compilation |
| Movement simulation runs inside local prediction | Restore preserves the enclosing host and current command; unavailable interfaces prevent entity access | Movement lifecycle regression and ownership/caller review |
| Bunny-hop choking reads flags after skipped prediction | The comparison requires an active prediction pass | Caller review and inactive-state assertions |
| A target occupies player slot 1 | Rapid fire and target-lock timing accept that slot | Both target-index gates reviewed |
| A configured key is negative, oversized or would narrow to another key | Keyboard queries reject it before indexing state | Invalid-key boundary regression |
| Names or labels contain percent characters | Display text uses a literal format argument | Deferred-text behavioral test and audit of all draw calls |
| Deferred text references a temporary or subsequently modified string | The queue owns a copy | Temporary-string and mutation regression |
| Display text exceeds the fixed buffer | Narrow and wide text truncate safely | Long-text regression for both overloads |
| A deferred clip rectangle starts at x = 0 | Its dimensions enable clipping and clipping is released afterward | Screen-edge clip regression |
| Configuration numbers exceed their storage type or colors leave byte range | Invalid values leave the setting unchanged; other valid settings still load | Numeric limits, overflow and atomic-color tests |
| Skin profile integers wrap or legacy attributes overflow | Numeric fields validate before conversion; invalid legacy values cannot enable flags | Three real file-loading, migration and persistence tests |
| Chat input is unquoted, empty, malformed or contains another command | Only complete chat commands are rewritten; quoting and other commands survive | Three chat-command behavioral tests |

All six changed regression files are explicitly included in the Google Test
project. The audit added 17 behavioral tests. Existing worktree changes were
preserved, and CI snapshots used a separate temporary Git index.

## Validation

Local compilation is disallowed by the repository instructions. All compilation
and compiled tests run in GitHub Actions. The game was not launched.

- Final test snapshot: `d8c1feb0314a13e67e7aac00d0bc564e7d9785c3`.
- [Full test run](https://github.com/Tsuki321/SEOwnedDE-Rei-Version/actions/runs/44239461091): all 621 tests in 145 suites passed, including all 17 new regressions.
- [DLL build](https://github.com/Tsuki321/SEOwnedDE-Rei-Version/actions/runs/44239257124): passed, snapshot `a69eaffc480ece47d0148ecdc26cfa574cf2325c`.
- The DLL snapshot and final test snapshot have identical `SEOwnedDE` trees; the
  later commit only corrects the migration test's schema IDs and canonical item ID.
- Project XML contains no missing source/header entries. Every remaining dynamic
  draw format is the intentional numeric slider format with its supplied value.
- A reconstructed worktree snapshot matched the final CI source and test snapshot,
  and the whitespace check passed. This report is the only subsequent change.

Tests exercise headless behavior and contracts. Engine-dependent movement and
shutdown behavior was checked through source/caller review and DLL compilation;
in-game verification remains outside the user's requested validation scope.

The final review found no further concrete defects. Both required CI jobs are
complete and successful, the current production/test trees match their verified
snapshots, and no identified fix or validation remains pending.
