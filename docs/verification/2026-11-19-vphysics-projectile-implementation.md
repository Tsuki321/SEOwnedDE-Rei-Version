# VPhysics projectile implementation

This change makes trajectory simulation part of aiming rather than only a final rejection check. Analytic ballistics now provide an initial angle/time estimate with no scalar drag correction. A bounded numerical solver refines pitch, yaw, and flight time against the simulator. Pipe projectiles use VPhysics; other projectiles retain their analytical movement backend.

Each solve evaluates the target at `timing bias + remaining charge wait + flight time`. Direct, charged, and alternative arc candidates are generated independently. Candidates are refined and collision-checked before selection, and the final score uses the actual first impact time. The configured arc has priority over its fallback. Multipoints and splash can still be evaluated after baseline failure.

Native simulation retains its environment across trials. Bodies are recreated for each trial, avoiding reuse of hidden body integration state. The empty environment advances by one physics timestep before resetting its clock, establishing the same integration phase for fresh and reused trials. Model collision solids and their parsed mass/inertia/damping parameters are used when available; collision data owned by the model is borrowed, never destroyed by the predictor. An unavailable model uses the existing deterministic drag-basis fallback. The fallback is retried so a subsequently loaded model can replace it. Prediction environments copy the live gravity, air density, timestep, and performance settings. Level shutdown releases bodies before model data unloads.

Launch profiles centralize collision bounds, muzzle impulse, spin, lifetime, and charge mapping. Sticky charge speed applies the shared pipe firing path's `mult_projectile_range` attribute, including at partial charge; bows retain their own charge remap. Center-fired weapons retain forward/vertical muzzle offsets. Non-pipe convergence uses an eye trace and the relevant teammate filter. Pipe muzzle placement uses the brush obstruction hull and rejects a blocked launch, following Valve's published setup behavior.

Target contact is swept across each projectile segment and translating target pose. A target hit before a later wall impact is accepted. Bow head aim checks the first intersected captured hitbox; it does not treat broad player bounds as proof of a headshot. Hitbox matrices are translated with the predicted player origin; future skeletal animation is not forecast. The movement-history score is labeled "Movement Confidence" in the menu.

The numerical solver limits iterations and shares a per-target budget of 160 trajectory trials and 12000 simulation steps with collision validation. Candidates whose remaining wait already exceeds the best viable score are skipped. Analytic seeds, target paths, and cached model shapes reduce redundant work.

Eleven behavioral regression tests cover nonlinear drag and muzzle offsets, charge/latency clocks, unreachable horizons, invalid input, budget exhaustion, continuous target contact, charge mapping, blocked candidates, and ranking after validation. The nonlinear backend in these tests is synthetic; it is explicitly not a captured retail trajectory.

`CProjectileSim::CaptureSnapshot` exposes mass, inertia, axis drag responses, gravity, density, and timestep through public physics APIs for future live calibration. The installed build's native drag and parser layouts were inspected in IDA during the review. Live trajectory captures and in-game performance measurements remain necessary to establish retail accuracy and cost.

## Native verification

The installed `vphysics.dll` was loaded in an isolated Python process, without launching or attaching to TF2. Model collision solids and their parameters were read from the installed game's VPK using the native loader and key parser. For grenade, sticky, and cannonball bodies, 60-tick trajectories were compared between repeated trials and fresh environments, including reuse of one environment across different models.

Without empty-environment priming, repeated trajectories differed by up to 18.0894 units. With the corrected reset sequence, all three models produced exactly identical samples: maximum repeated-trial and fresh-environment error was **0.0 units**. All three model solids have mass 5, inertia scale 1, and zero linear/rotational damping. The captured axis drag responses agree with the installed model geometry.

The reproducible harness is `tools/verification/capture_projectile_physics.py`. Numerical captures and the DLL hash are in `docs/verification/data/2026-11-19-native-projectile-physics.json`. These checks verify the native reset sequence and model data, rather than a complete spawned server projectile, weapon RNG, or in-game collision behavior.

Additional captures compared yaw impulses 0, 600, -600, and 1200. At 60 ticks, grenade endpoints differed by 65.14 units between yaw 0 and 1200, versus 5.83 units between 600 and 1200. Sticky/cannon differences were 28.69/45.86 units for 0 versus 1200, and 3.39/3.95 units for 600 versus 1200. The deterministic profile now uses pitch/yaw/roll impulse `600/600/0` as a representative spun trajectory, and respects `grenade_no_spin`. This is an approximation of shot randomness, not prediction of the server RNG. The capture tool's `--spin-yaw` option allows reproducing these comparisons.

## CI verification

Source commit `0c0d50abb3e1a8ed59abdfaaf2c701acef7239e3` passed both checks:

- [ReleaseAVX2 x64 DLL build and artifact upload](https://github.com/Tsuki321/SEOwnedDE-Rei-Version/actions/runs/44485954846).
- [Full Google Test suite: 648 tests from 157 suites passed](https://github.com/Tsuki321/SEOwnedDE-Rei-Version/actions/runs/44485954824), including the 11 new trajectory regressions.

No local compilation was performed. The native verification harness executed the already installed game DLLs in isolated processes. In-game projectile captures, exact weapon RNG, and performance under real target loads remain separate verification work.
