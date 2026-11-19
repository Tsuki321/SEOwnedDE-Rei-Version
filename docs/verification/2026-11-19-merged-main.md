# Consolidated main: backtrack and native cosmetics

All local and fetched origin/upstream branch tips are ancestors of `main`.
The pre-existing workspace was preserved in `77932a5` before the merges.
Overlapping older pose and skin changes were resolved in favor of the newer
verified implementations; the x64 smooth target-switch delay is retained.

## Manual hitscan

The installed x64 TF2 client has SHA-256
`b6f45c74805edacf0cf61d965f658158fc22960f66d583ef3a6549eb56696b95`.
Read-only IDA MCP decompilation confirms:

- `C_BaseAnimating::TestHitboxes` at `0x1801d62b0` reads the compact hitbox cache
  through `C_BaseAnimating::GetBoneCache` at `0x1801cdce0`.
- That cache is independent of the `m_CachedBoneData` matrices previously
  replaced by `CLagRecordMatrixHelper`. A valid compact cache bypassed SetupBones
  and retained the live pose, explaining historical traces that missed or passed
  through the displayed history.
- The validity function at `0x1801bfe20` compares the first float in CBoneCache
  with curtime; the updater at `0x1801c4e30` writes that float after copying bones.
- Both scope activation and restoration now invalidate this cache. The next
  trace rebuilds it from the currently installed matrices. The getter signature
  has one match in the installed binary.

Historical ray culling happens before cache replacement. Existing server rewind
limits, teleport rejection, shot ownership, and world occlusion remain in force.
The predicted shot is resolved before prediction ends; subsequent seed-spread
compensation preserves its intended direction. In-game hit registration still
requires server verification.

## Native backpack and cosmetics

`NativeCosmeticBuild.h` records verified function addresses and byte prefixes.
The adapter requires the matching PE timestamp, image size, and every prefix;
an unsupported client build disables the native adapter with a status message.
It creates CEconItem objects with the game's factory, adds item views through
CPlayerInventory::AddEconItem, and supplies their data and per-class loadouts to
the native UI. It does not add objects to Steam's SOCache or send synthetic equip
IDs to the game coordinator. Owned item queries retain their normal paths.

Synthetic IDs use a separate prefix. Items are non-tradable and non-craftable.
Multi-class equip state is populated through the verified CEconItem::Equip
function. View pointers are reacquired after the native inventory vector changes.
Fake views and objects are removed on disable/unload; native view rebuilds and
Steam account changes are handled on the frame thread.

The backpack expander uses CTFPlayerInventory::GetMaxItemCount. The supported
panel maximum is **4000 slots** (80 pages of 50); real item positions are kept,
and simulated items occupy unoccupied slots starting from the end. Expansion
is local to the client's display, and is independently persisted in the cosmetic
inventory settings.

## Selectable crates

The catalog is read through IBaseFileSystem, covering loose files and VPKs.
The menu has searchable, paginated crate selection. Pools resolve crate series,
prefabs, nested client loot lists, and rarity groups in item collections; weapons
and empty cosmetic pools are filtered out. Each selected crate uses its own
cosmetic pool. These are uniform simulated drops, not a reproduction of Steam's
rarity probabilities. Successful unboxing/equipping enables the simulation.

## Verification

Project XML, source registration, whitespace, branch ancestry, and IDA signature
checks are performed without local compilation. DLL compilation and executable
tests run through GitHub Actions. Native UI interactions, death/respawn,
reconnect, multi-class rendering, and actual server hit registration need
in-game testing with the resulting DLL.

Primary source references:

- [Valve C_BaseAnimating cache implementation](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/client/c_baseanimating.cpp)
- [Valve native inventory implementation](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/econ/econ_item_inventory.cpp)
- [Valve TF2 inventory implementation](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_item_inventory.cpp)
- [Valve backpack limits](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/econ/econ_item_constants.h)
