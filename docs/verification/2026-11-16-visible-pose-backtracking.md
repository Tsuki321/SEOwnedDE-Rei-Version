# Visible player interpolation and manual backtracking

The November 15 display-only bone blending could separate the body from its
cosmetics and from the hitboxes used to resolve a manual shot. It also repeatedly
retargeted the blend on network updates. That code has been removed.

Accuracy Improvements remains available. Players now use the engine's normal
interpolation in that mode as well. Pose, cycle, velocity and eye-angle watchers
are always registered, so weapons and cosmetics follow the same body animation.
Door interpolation and the local recharge exception retain their existing behavior.

Live shot resolution and record capture use the last render frame's pose time.
CreateMove's predicted clock and the newest network simulation time cannot change
the timestamp attached to the body still visible on screen. Historical shots use
their own stored pose time. Other live tick writers and projectile prediction use
the same interpolated-player convention.

Manual hitscan backtracking keeps its own enable setting and works with the
aimbot disabled. Its capture and scan no longer require the aimbot's player and
lag-record targeting toggles. In Accuracy mode, a direct hit on the visible body
wins; a miss can search recent records. This does not require visible backtrack
ghosts or change the command's aim angles. Existing occlusion checks and the
record age limit remain in place.

## IDA verification

The existing Steam x64 client database was reopened through IDA MCP. The
`DT_BaseEntity` receive-table initializer at RVA `0x1DC670` identifies network
`moveparent` at offset 572. `SetAbsOrigin` at RVA `0x1E6E30` uses the current parent
handle at 556. `InvalidatePhysicsRecursive` at RVA `0x1A9A90` walks child 560 and
peer 564. Consequently `GetMoveParent` needs `moveparent - 16`, while the existing
child and peer offsets remain correct. The previous `-4` selected the wrong field.
These offsets were checked against the binary rather than copied from Amalgam.

## Validation and usage

GitHub Actions builds and runs the regression suite. New clock tests cover the
visible-pose timestamp, prediction advancing between render frames, changing lerp,
low/high frame rates, disconnects and invalid clocks. Interpolation and manual
resolver integration checks were updated for the corrected behavior. Earlier
skin fixes remain covered.

**Reconnect to the server after replacing the DLL.** Existing entities created by
the old DLL can still lack interpolation watchers; changing the draw hook alone
does not recreate them. Keep Accuracy Improvements and Manual Backtrack enabled;
backtrack ghosts can remain disabled.

In-game hit registration and visual smoothness still require a play test.
