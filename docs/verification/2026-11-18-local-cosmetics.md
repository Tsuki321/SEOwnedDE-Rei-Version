# Local cosmetic simulation

Open **Skins > Cosmetics**, choose a drop pool, and click **Open simulated crate**.
Each opening creates a new inventory instance, including when the same cosmetic
drops twice. Select an instance, choose a loadout class and slot, and click
**Equip selected**. Enable **Preview on character** and use third person to see it.
There are three slots for each of the nine classes. Equipping an incompatible
class is rejected; equipping an overlapping region replaces the conflicting
simulated cosmetic. **Unequip slot** clears a slot. Removing an item requires a
second click and clears every loadout referencing that instance.

The drop pools use uniform sampling of wearable definitions in the installed
`tf/scripts/items/items_game.txt`. They are free simulated pools with independent
drops. The inventory appears in this menu and is saved in the config folder as
`simulated_cosmetics.json`. It does not modify the Steam backpack or send economy
messages to the game coordinator. A failed load preserves the original file;
successful saves write a temporary file and replace the previous file atomically.
The inventory holds up to 4096 instances.

The renderer creates client-only `C_TFWearable` entities, registers model paths as
client-side dynamic models, and attaches them to the local character with bone
merging. It uses the red/blue skin associated with the local team. It clears
previews on death, class/team changes, disconnection, disabling the preview,
cloaking, disguising, and clean screenshot frames. Unload waits for cleanup on
the client frame thread. **Hide owned cosmetics** suppresses local character
wearables during model draws when a simulated preview is present. It defaults
to off. Bodygroup changes to the player's head/face and unusual particles are
outside this first implementation; default cosmetic model styles are used.

## Evidence and remaining checks

- IDA MCP is serving the installed x64 `client.dll` using a task-owned database
  under `%TEMP%/seowned-cosmetics-ida`. The original Quick start instance was left
  open. No patches were applied to the binary.
- The three runtime signatures each match exactly one executable address in
  that binary. Recorded patterns, addresses, and the binary SHA-256 are in
  `2026-11-18-local-cosmetics-signatures.json`.
- IDA confirms the `C_TFWearable` factory constructs a 3808-byte entity without
  registering it as a network entity. Vtable slot 82 is the `C_EconEntity`
  client-only initializer; the base initializer adds a non-networkable entity
  and renderable to the client systems. `FollowEntity` establishes parenting and
  bone merging. `SetModelByIndex` writes the engine's 16-bit model field.
- `DT_ScriptCreatedItem` registration at `0x18001DB00` declares
  `m_iItemDefinitionIndex` at item-view offset 72 with width 2. The weapon SDK
  accessor and skin changer references now use `uint16_t` as well. A compile-time
  assertion guards this width.
- Twelve new C++ tests cover prefab/class model resolution, invalid model paths,
  schema failures, crate pools, duplicate instances, equip conflicts, item
  removal, capacity, persistence, and stale loadout cleanup. Both projects include
  the new files. The XML, source paths, and whitespace checks passed locally.
- **Pending:** GitHub Actions DLL build and compiled tests. Local compilation is
  prohibited by the repository and the no-local-compilation skill.
- **Pending:** in-game checks with the compiled DLL: hats and glasses on multiple
  classes, red/blue skins, movement and third-person bone merging, replacement
  of owned wearables, death/respawn, class/team changes, disconnect/reconnect,
  screenshots, and unload. Static disassembly does not prove these behaviors.

Preview signatures are resolved lazily. If one is absent on another client build,
the inventory remains usable and the menu reports unavailable previews.
