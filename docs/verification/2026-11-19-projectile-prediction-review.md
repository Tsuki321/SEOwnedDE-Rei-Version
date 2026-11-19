# Projectile prediction review and VPhysics integration proposal

Reviewed on 2026-11-19. Scope: projectile aiming, projectile simulation, movement prediction, charge planning, splash selection, and their tests. This report includes static analysis of the installed 64-bit TF2 binaries using IDA Pro 9.3 through MCP. Prediction code has not been changed. No local build, test-suite execution, or live-game trajectory measurements were performed.

## Assessment

The strongest improvement is to make the existing VPhysics simulator the shared trajectory model for solving and validating pipe projectiles. At present, the solver aims using an approximate scalar drag model, and VPhysics subsequently accepts or rejects that aim. A rejected trajectory never feeds its measured error back into the solver.

Keep the analytic solver as a fast initial estimate. For grenades, stickies, and cannonballs, refine that estimate using native simulation. Rockets and other projectiles currently simulated analytically should retain a backend appropriate to their movement type; stronger VPhysics integration does not require assigning every projectile VPhysics movement.

Several existing protections are useful and should survive the refactor: double-precision polynomial calculations, positive-root enumeration, drag-convergence checks, half-tick temporal tolerances, movement-state restoration through RAII, target pose snapshots, and swept obstacle traces.

## Prioritized findings

### 1. High: the solver's drag model differs fundamentally from native VPhysics

**Locations:** `AimbotProjectilePrediction.h:145`, `:166`, `:619`; `ProjectileSim.cpp:289`; `AimbotProjectile.cpp:448`.

The solver averages each weapon's three drag-basis components, multiplies the result by air density, and uses an exponential effective-speed correction. It continues to model gravity and muzzle impulse separately in its ballistic equation.

IDA confirms that native drag uses velocity components transformed into the body's frame. Its drag controller computes a velocity-dependent decrement and applies that decrement to velocity, with a clamp preventing reversal. The body's orientation and angular velocity therefore matter. This is not the constant-rate exponential model used by the solver.

`CanArcReach` checks the resulting VPhysics endpoint but does not correct the launch angle or meeting time. Consequently, an accurate native simulation can reject a shot that a native refinement could have recovered. Increasing polynomial precision or the exponential iteration count cannot resolve the model disagreement.

**Recommended change:** solve pitch, yaw, and meeting time against the native trajectory, starting from the analytic estimate. Measure both spatial and temporal residuals. Use bounded finite-difference updates with damping or a bracketed fallback, and validate each accepted result with the same launch state and simulator.

### 2. High: charge and splash searches depend on baseline success

**Locations:** `AimbotProjectile.cpp:725`, `:767`, `:777`, `:810`, `:893`, `:963`; static-target branch at `:974`.

The player branch rejects an invalid baseline meeting solution before reaching charge planning or either splash mode. It also requires a successful baseline launch solve. The static-target branch similarly returns immediately when the direct launch solve fails, before considering splash.

A current-charge trajectory outside the solver's range or prediction horizon can prevent evaluation of a charged alternative. A failure to solve the requested direct aim point can also prevent evaluation of nearby splash surfaces. This matters especially while replacing the approximate pipe solver: failure of its estimate should not prove that all native trajectories are unreachable.

**Recommended change:** generate direct, charge, multipoint, and splash candidates independently where enabled. Treat an invalid baseline as an unavailable candidate. Each alternative should establish its own reachable meeting time within the configured horizon.

### 3. High: charge candidates are ranked before their trajectories are validated

**Locations:** `AimbotProjectile.cpp:833`, `:868`, `:900`, `:905`.

The charge loop keeps the fastest analytic candidate. Obstacle and native endpoint validation occur after the loop. If that candidate fails, the code restores the baseline; it does not revisit another charged candidate that may clear an obstacle or produce a valid native endpoint.

**Recommended change:** retain a small ordered candidate set and validate candidates in score order. Commit aim, charge time, target pose, and launch state only after validation succeeds. This also removes the need to restore several mutable baseline fields manually.

### 4. High: collision geometry and launch setup need a shared, verified description

**Locations:** `ProjectileSim.cpp:235`, `:243`, `:289`; `AimbotProjectile.cpp:110`; `SDK.h:90`; `ProjectileSim.cpp:60`.

The simulator creates one fixed cube with bounds of +/-2 units, zeroes several inertia-related creation parameters, then overwrites drag bases with weapon constants. Separately, collision validation uses +/-8 units horizontally and +/-20 vertically for pipes, and +/-6 for most other projectile types. These geometry choices serve different operations but currently lack a documented relationship to actual projectile geometry.

Native drag-basis recomputation depends on collision geometry, inverse mass, and inertia. Writing measured bases into a generic body reproduces those numbers, but does not establish equivalence of the complete spinning body.

The reconstructed muzzle setup always converges non-pipe launches toward a point 2000 units away. Valve's published implementation traces the eye ray and uses the resulting endpoint when appropriate. Its `centerfire_projectile` handling adjusts the lateral offset, whereas the local Original special case replaces the entire muzzle position with the shoot position. These are verified source differences; their exact retail behavior still needs a binary or live comparison. [Valve weapon setup](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weaponbase.cpp)

Valve's published rocket implementation uses zero-sized collision bounds, which also differs from the local generic +/-6 hull. Confirm each retail projectile's collision behavior before changing these values. [Valve rocket implementation](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weaponbase_rocket.cpp)

**Recommended change:** define one projectile profile containing movement backend, launch rules, collision shape, mass/inertia parameters, drag configuration, spin policy, charge mapping, and lifetime. Obtain model collision data through existing `GetVCollide` interfaces where appropriate. Keep simulation-body geometry and gameplay collision bounds explicit. Prefer public physics methods; isolate any necessary private-field access in a build-verified adapter.

### 5. Medium: every native trial destroys and recreates the physics environment

**Locations:** `ProjectileSim.cpp:209`, `:218`, `:235`; `ProjectileSim.cpp:12`.

Every VPhysics initialization after the first calls `CleanUp`, destroying the object, collide object, and environment. This adds allocation and initialization work to every trajectory trial and would become substantially more expensive with native refinement.

**Recommended change:** retain an isolated prediction environment and cache owned collision shapes by profile. First reuse the environment while recreating only the body when needed. Then evaluate safe body reuse through a full reset or native serialization. Verify that repeated trials match fresh-object trials; `SetPosition`, velocity assignment, and clock reset alone should not be assumed to reset every internal state.

Copy relevant live environment settings, including air density, gravity, and simulation timestep, rather than relying on a mixture of hardcoded values and new-environment defaults. Measure allocation counts and simulation time before claiming a speedup.

### 6. Medium: direct-hit validation only checks target overlap at the endpoint

**Location:** `AimbotProjectile.cpp:448`.

The target is excluded from obstacle traces, then the final projectile position is tested against the expanded predicted target bounds. The code does not calculate the first intersection with the moving target during each segment.

A projectile can enter the target bounds before the nominal endpoint and subsequently reach an obstacle that causes rejection. Conversely, broad endpoint overlap does not establish which hitbox a bow trajectory hits first.

**Recommended change:** sweep against interpolated target bounds during each projectile step, compare target-contact time with obstacle-contact time, and return the first relevant impact. Use hitbox-aware validation where a headshot is required. Share this impact result with direct and splash evaluation.

### 7. Medium: search results and charge mapping need clearer contracts

**Locations:** `AimbotProjectile.cpp:725`, `:814`, `:824`; `AimbotProjectilePrediction.h:689`, `:765`, `:828`.

Path scanning accepts an approximately matching tick, then runs local refinement. It does not preserve alternative meeting intervals or arc choices when the selected result fails trajectory validation. The binary-search helper is not used by the production path, and an option selecting a high arc does not itself provide a low/high candidate fallback.

Charge planning also duplicates speed/gravity formulas separately from `ProjectileSim::GetInfo`. Both sticky launch extraction and candidate planning used fixed 900-2400 speed endpoints without applying `mult_projectile_speed`. On weapons with such attributes, the predicted launch can differ from the actual weapon.

**Recommended change:** centralize charge-to-launch conversion in the projectile profile, including attributes. Return candidate intervals and arc choices under a bounded work budget. Replace loosely related floats and booleans with a result containing launch state, flight time, remaining charge wait, target time, spatial error, timing error, and a failure reason.

### 8. Medium: tests do not establish agreement with native trajectories

**Locations:** `tests/cpp/features/TestProjectilePrediction.cpp:649`; `TestProjectileSim.cpp`; `TestAimbot.cpp:193`.

The math suite includes useful numerical cases. However, drag-consistency tests compare the scalar coefficient with the same averaged basis formula, and multiple simulator/aimbot integration checks search for source strings. They can pass while the native trajectory disagrees or a candidate is rejected incorrectly.

**Recommended change:** add behavior tests for independent charge/splash search, blocked preferred charge candidates, attribute-adjusted charge mapping, first target contact, and repeated simulator reset. Capture actual projectile trajectories for native regression fixtures. String-presence checks should not carry the accuracy claim.

The current hitchance calculation is a movement-history consistency score, not a simulated probability of collision. It returns full confidence with fewer than three records. Rename or document that score until trajectory variation is actually sampled.

## IDA evidence and MCP status

The configured MCP supervisor is running at `http://127.0.0.1:13337/mcp`, with analyzed `server.dll` and `vphysics.dll` sessions. Analysis used copies under `%TEMP%/seowned-projectile-vphysics-20261119`; installed game DLLs were not modified.

Installed Steam build: **26334388**. IDA image base for both DLLs: `0x180000000`. Names below are interpretations supported by call sites, signatures, and interface slots; the retail binaries are not fully symbolized.

| Binary | Address | Evidence |
| --- | --- | --- |
| vphysics.dll | `0x18001B340` | Existing physics-object creation signature matches exactly once. |
| vphysics.dll | `0x18001D4E0` | Existing drag-base recomputation signature matches exactly once; writes linear bases at `0x28`, `0x2C`, `0x30` and angular bases at `0x34`, `0x38`, `0x3C`. Geometry, inverse mass, and inertia contribute to recomputation. |
| vphysics.dll | `0x180016370` | Native drag-controller simulation applies velocity-dependent, clamped linear and angular decrements. |
| vphysics.dll | `0x18001BC60` | Internal linear drag helper transforms velocity into the body's frame and evaluates weighted absolute components. |
| vphysics.dll | `0x18001BB20` | Internal angular drag helper evaluates angular velocity components against the angular basis. |
| vphysics.dll | `0x18001AE80` | Public `CalculateLinearDrag`, object vtable slot 64. |
| vphysics.dll | `0x18001AC00` | Public `CalculateAngularDrag`, slot 65; includes degree/radian conversion. |
| vphysics.dll | `0x18001DBF0` | `SetDragCoefficient`, slot 36; writes coefficients at `0x58`/`0x5C` and updates drag enablement. |
| vphysics.dll | `0x18001E170` | `SetVelocityInstantaneous`, slot 50; converts Source units and writes simulation velocity immediately. |
| server.dll | `0x180747DD0` | Pipe projectile spawn selects grenade, sticky, or cannonball models. |
| server.dll | `0x180778F90` | Shared grenade spawn setup supplies +/-2 bounds and invokes native physics initialization. |
| server.dll | `0x180266000` | Native physics initialization creates a physics object through the entity/model path. |
| server.dll | `0x180359020` | The creation path selects a solid from model collision data and passes its parsed parameters to the physics environment. |

SHA-256:

```text
vphysics.dll 72947CE83C9C6F99B8ED7DB61580F6FD616B1C49CDD66EF1202C1E2824368827
server.dll   C0EE3A93F5532CEDF182F822ECA5BD691ED7290FAD32ED766BE99AD690E1C90D
```

Native recomputation confirms the current drag-basis offsets for this build. It does **not** verify the hardcoded per-weapon basis values, the zeroed inertia setup, or the assumption that random yaw spin has negligible trajectory effects. Those need measurements from freshly spawned projectiles.

## Proposed implementation sequence

1. **Establish reference profiles.** Capture live launch position, velocity, spin, bounds, model collision data, mass, inertia, damping, axis drag responses, and environment settings. Record weapon attributes and binary/build identity with each capture.
2. **Refactor simulation ownership.** Separate persistent environment ownership from per-trial body state. Add a trajectory result with timestamps, position/velocity samples, and first-impact information. Verify reset equivalence before relying on reuse.
3. **Add native refinement.** Use the analytic solver to seed pipe candidates, then minimize spatial and temporal error through the isolated VPhysics environment. Preserve an explicit simulator-call budget and alternative candidates.
4. **Unify candidate evaluation.** Evaluate direct, charged, multipoint, arc, and splash candidates through the same launch/profile and impact evaluator. Rank only viable candidates and commit one complete result.
5. **Validate through CI and live captures.** Build/test through GitHub Actions. Compare native predictions with captured game trajectories across charge levels, pitch, spin, server gravity, nearby surfaces, and moving targets. Track endpoint error, impact-time error, solver success rate, and cost per command.

The first implementation milestone should be demonstrable agreement between one pipe profile's native prediction and recorded game trajectories. That gives subsequent solver and performance refactors a reliable reference.
