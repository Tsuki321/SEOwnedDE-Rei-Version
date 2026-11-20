# Hitscan assist and triggerbot verification

Aim Assist is an independent, default-on Hitscan toggle. Plain, Silent, and
Smooth remain the keyed aim modes. The global Aimbot master still applies;
ordinary keyed aim takes precedence over assistance. Assist itself never fires
or stamps command ticks.

The default damping is 15 (higher is gentler). Assistance acquires within at
most 3 degrees, requires 100 ms of continuous candidate eligibility, and keeps
the same serial-inclusive entity handle while visible and eligible. It retains
the lock outside its influence cone without pulling the view back. Movement is
time-based, filtered, limited to 10 degrees/second at the default, and bounded
in acceleration and overshoot. Valid interior crosshair hits need no correction.
Dead, dormant, ignored, occluded, or off-screen targets release the lock; a new
candidate must settle before corrections resume.

AutoShoot now traces the outgoing command plus recoil over weapon range, then
requires a strict interior intersection of the native hitbox selected by that
trace. Inset scales shrink around the actual local midpoint and are capped at
1. Bone matrix inversion preserves rotation and model scale. Failed bone setup,
invalid set/hitbox/bone indices, nonfinite geometry, grazing contacts, and
out-of-range intersections fail safely. Wait for Headshot requires a head
contact as well as a ready headshot weapon. Manual shot ownership, live tick
resolution, ammo readiness, and the aimbot fire-delay gate remain in place.

The menu measures its content height, clamps its window to the display, and
clips a scrollable content viewport. Mouse wheel, high-resolution wheel
remainders, and scrollbar dragging reach long pages. Dropdowns remain inside
their viewport and surface clipping state is restored. Legacy saved mode 3
loads as Smooth with independent assist enabled; explicit assist-off persists.

## Native binary evidence

IDA Pro 9.3 idalib runs headlessly through IDA MCP at
`http://127.0.0.1:13337/mcp`. Sessions `hitscan-client` and `hitscan-engine` use
task-owned database copies under `%TEMP%/seowned-hitscan-assist-20261120`.
Installed Steam build: **26334388**. Image base: `0x180000000`.

| Binary | SHA-256 |
| --- | --- |
| client.dll | `B6F45C74805EDACF0CF61D965F658158FC22960F66D583EF3A6549EB56696B95` |
| engine.dll | `4802A74CE0ABB1175851D00E37FC88609085A96238AA52AC0EB224AE4A08EC80` |

Interpreted client routines, verified from the installed DLL:

| Address | Interpretation and evidence |
| --- | --- |
| `0x1801D62B0` | `C_BaseAnimating::TestHitboxes`: current studio hitbox set, independent bone cache, per-bone matrices. |
| `0x1801C48F0` | `ClipRayToHitboxes`: 68-byte hitbox records, each record's bone index, nearest finite ray fraction, model-scale handling. |
| `0x1801BE390` | `IntersectRayWithHitbox`: transforms the local midpoint and half extents, uses matrix columns, then tests the finite ray in bone-local space. |
| `0x180256CF0` | `IntersectRayWithBox`: bounded ray fraction, startsolid handling, local box plane result. |

The capture script maps a **DLL copy** with `DONT_RESOLVE_DLL_REFERENCES` and
calls only the verified pure intersection routine and its direct math callees.
DllMain and imported client interfaces are not initialized; no game process is
injected or modified. A SHA-256 mismatch refuses the hardcoded address.

**324 native calls captured**, including 45 hits and 279 misses, cover three
rotations, three model scales, two inset scales, asymmetric bounds, and short
versus sufficient weapon range. The C++ regression test replays these fixtures
against the production `Math::RayToHitbox`. Exact tangent cases are tested
separately because strict interior checks intentionally reject some contacts
the engine's tolerance can accept.

Reproduce the capture on Windows/x64 with the verified DLL copy:

```powershell
python tools/verification/capture_hitscan_geometry.py --client-copy "$env:TEMP/seowned-hitscan-assist-20261120/client.dll" --output docs/verification/data/2026-11-20-native-hitscan-geometry.json
```

## Validation scope

Local native capture, project XML/path checks, and whitespace checks passed.
DLL compilation and compiled behavioral tests run exclusively in GitHub
Actions. Their final results are recorded after the runs finish.

These checks verify geometry and deterministic controller behavior. Live
camera feel, menu operation in-game, random weapon spread, network delay,
server lag compensation, and actual damage registration still require a game
session. No hit-rate improvement percentage is claimed.
