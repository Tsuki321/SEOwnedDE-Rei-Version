# Accuracy animations, signatures and skin application

Reference: [rei-2/Amalgam at da2fae6319deabb603eb51eb281c587f0dfb48c8](https://github.com/rei-2/Amalgam/tree/da2fae6319deabb603eb51eb281c587f0dfb48c8).

## Behavior

- Accuracy Improvements still suppresses gameplay interpolation and captures network-time poses for hitscan/manual backtracking.
- Live player models and bone-merged attachments blend private display matrices between snapshots. Rotations use normalized quaternion interpolation; scale is preserved. This adds up to a snapshot of visual delay (blend duration capped at 100 ms).
- Historical chams bypass smoothing inside the existing lag-record scope. Display matrices never replace engine bone caches or recorded hitboxes.
- Model/handle changes, time rewinds, teleports and gaps in rendering reset the blend. Bones unavailable at a previous LOD use the current engine transform.
- Skin application resolves attribute definitions through the current indexed lookup. Attribute matching reads the 16-bit definition ID without its padding. Integer schema attributes (paint kit, seeds, team color, Australium, rarity and Pip-Boy flag) preserve integer bits through the float setter.
- KeyValues now has the current 72-byte layout and uses the engine allocator, matching the engine's ownership of material objects. Names are passed directly, without interpreting them as format strings.

## Binary verification

The installed Steam x64 binaries were loaded into IDA Pro and queried through its MCP server. All 82 client and 14 engine `MAKE_SIGNATURE` patterns matched exactly once after the changes. A separate executable-section scan checked all 101 function patterns and all eight signature-based interfaces; each matched once.

| Symbol | Finding | Replacement / evidence |
| --- | --- | --- |
| `KeyValues_Initialize` | Old signature had no match | Amalgam signature, engine RVA `0x287790`; constructor writes through offset `0x40`, requiring 72 bytes |
| `CBaseEntity_SetAbsOrigin` | Amalgam's short prefix matched twice | Extended through `0F 2E 07`; unique client RVA `0x1E6E30`; decompilation confirms origin updates |
| `CEconItemSchema_GetAttributeDefinition` | Old tree-lookup signature had no match | Client RVA `0x391C30`; bounds-checked pointer array at `0x2D8`, count at `0x2E8`; callers use the result as an attribute definition |
| `CAttributeList_SetRuntimeAttributeValue` | Signature still valid | Client RVA `0x3A3260`; definition ID read as WORD, value stored as raw 32 bits |

SHA-256:

- `tf/bin/x64/client.dll`: `b6f45c74805edacf0cf61d965f658158fc22960f66d583ef3a6549eb56696b95`
- `bin/x64/engine.dll`: `4802a74ce0abb1175851d00e37fc88609085a96238aa52ac0eb224ae4a08ec80`

Integer encodings were checked against the installed `tf/scripts/items/items_game.txt`, including `stored_as_integer` for attributes 295, 745, 834, 866, 867, 2022 and 2027.

## Validation

Compilation and Google Test execution use GitHub Actions only. New behavioral tests exercise display-only copies, blending, repeated draw passes, reset conditions, LOD changes, rotation/scale, padded attribute IDs and integer value encoding. Existing backtracking and skin lifecycle regression tests remain enabled.

The binary audit and automated tests do not replace in-game verification. Check Accuracy Improvements with moving/turning players, weapon attachments and historical chams; verify manual hitscan backtracking; apply/change/remove skins, seeds and Australium, then switch weapons and respawn.
