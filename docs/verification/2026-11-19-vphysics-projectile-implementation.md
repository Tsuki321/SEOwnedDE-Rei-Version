# VPhysics projectile implementation

This change makes trajectory simulation part of aiming rather than only a final rejection check. Analytic ballistics now provide an initial angle/time estimate with no scalar drag correction. A bounded numerical solver refines pitch, yaw, and flight time against the simulator. Pipe projectiles use VPhysics; other projectiles retain their analytical movement backend.

Each solve evaluates the target at `timing bias + remaining charge wait + flight time`. Direct, charged, and alternative arc candidates are generated independently. Candidates are refined and collision-checked before selection, and the final score uses the actual first impact time. The configured arc has priority over its fallback. Multipoints and splash can still be evaluated after baseline failure.

Native simulation retains its environment across trials. Bodies are recreated for each trial, avoiding reuse of hidden body integration state. Model collision solids and their parsed mass/inertia/damping parameters are used when available; collision data owned by the model is borrowed, never destroyed by the predictor. An unavailable model uses the existing deterministic drag-basis fallback. The fallback is retried so a subsequently loaded model can replace it. Prediction environments copy the live gravity, air density, timestep, and performance settings. Level shutdown releases bodies before model data unloads.

Launch profiles centralize collision bounds, muzzle impulse, spin, lifetime, and charge mapping. Sticky charge speed now applies projectile speed attributes, including at partial charge. Center-fired weapons retain forward/vertical muzzle offsets. Non-pipe launch convergence uses an eye trace, following Valve's published setup behavior.

Target contact is swept across each projectile segment and translating target pose. A target hit before a later wall impact is accepted. Bow head aim checks the first intersected captured hitbox; it does not treat broad player bounds as proof of a headshot. Hitbox matrices are translated with the predicted player origin; future skeletal animation is not forecast. The movement-history score is labeled "Movement Confidence" in the menu.

The numerical solver limits iterations and shares a per-target budget of 160 trajectory trials and 12000 simulation steps with collision validation. Candidates whose remaining wait already exceeds the best viable score are skipped. Analytic seeds, target paths, and cached model shapes reduce redundant work.

Eleven behavioral regression tests cover nonlinear drag and muzzle offsets, charge/latency clocks, unreachable horizons, invalid input, budget exhaustion, continuous target contact, charge mapping, blocked candidates, and ranking after validation. The nonlinear backend in these tests is synthetic; it is explicitly not a captured retail trajectory.

`CProjectileSim::CaptureSnapshot` exposes mass, inertia, axis drag responses, gravity, density, and timestep through public physics APIs for future live calibration. The installed build's native drag and parser layouts were inspected in IDA during the review. Live trajectory captures and in-game performance measurements remain necessary to establish retail accuracy and cost.

Verification: GitHub Actions build and test runs are pending. No local compilation was performed.
