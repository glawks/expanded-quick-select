# RAC2 support (experimental: 2.0)

This branch keeps the tested RAC1 implementation intact and adds a separate
NTSC-U (v1.01) RAC2 runtime. It is **not a finished RAC2 release**.

## Implemented so far

- Multi-game manifest and runtime dispatch; the existing RAC1 branch remains unchanged.
- Game-specific maximum item ID (55 for RAC2, 39 for RAC1), with core tests.
- RAC2 sidecar data under the mod's `rac2/` folder, separate from RAC1's files.
- Opt-in Triangle + L1/R1 page switching using the SDK's active-low pad callback.
- A guarded, single-game-tick projection of RAC2's eight native Quick Select slots
  and its eight-entry HUD icon cache, with restoration after each game tick.
- Owned items in the game's inventory weapon/gadget order are put into the first
  empty extra-wheel slot if not already on an active page, including existing saves.
  Existing sidecar slots are preserved. Input is limited to the playing phase;
  unavailable sidecar items are masked from projection.
- A HUD text hint shows the current page; page-1 canonical save data is intended
  to remain untouched.

## Important limits / test warnings

- **RAC2 support is OFF by default** under the launcher setting
  `Enable experimental RAC2 pages`. Enable only to test; back up saves first.
- **Only NTSC-U v1.01** is declared. PAL and NTSC-U v2.00 have not been mapped.
- The wheel/pause-menu code does not have dedicated RAC2 Quick Select hooks in
  SDK 1.20.0; this first attempt uses `on_pad`, `on_frame`, and `Item_Give`.
  The pad-to-frame timing and exact HUD cache lifetime require in-game testing.
- **The pause-menu Quick Select editor is not ported.** Extra pages presently
  populate automatically from owned eligible weapons/gadgets, not through
  a fully featured editor. Existing saved extras load from the sidecars.
- On a HUD cache mismatch, the implementation skips writing game memory and
  warns once. This may prevent page switching until the correct cache
  relocation/layout is confirmed.
- The read-only EQS compatibility ABI remains RAC1-only in this prototype.

## Memory evidence (RAC2 NTSC-U v1.01)

Based on OpenRAC's `port/game/rac2-ntsc/host/rac2_addresses.h`
and `OpenRAC/rac2-decomp/nonmatching/game/func_0028BCE8.c`:

| Data | Address / layout |
| --- | --- |
| Canonical eight item slots | `g_equippedItemSlots = 0x001A73B8` (8 × u32) |
| HUD cache | `D_00255170` (8 × 0x1C; icon at +0, item at +0x18) |
| Item-to-record index | `g_itemEquippedSlot = 0x00139568` (u8 per item) |
| Item records | `g_weaponTable = 0x00239BA0` (0xE0 per record, u16 icon at +0x3C) |
| Owned flags | `g_inventoryOwned = 0x001A7AF8` (56 bytes) |
| Inventory order | `g_inventoryOrder = 0x001A7B68` (32 bytes, flagged items) |

All addresses and their relocation must be verified live before wider support.
Dev.2 restores projected slots before native item grants and skips cache restoration
across level-load generations; it is still a pad-to-frame projection, not a scoped
native wheel hook. Back up saves before testing.

## Suggested in-game test order

1. Back up RAC2 saves. Enable only Expanded Quick Select and its RAC2
   experimental setting, with two wheels.
2. Confirm startup logging and no regression when the experimental setting is off.
3. With items already equipped, hold Triangle and press R1. Verify the
   page hint and whether the wheel icons switch. Hold R1 to test edge detection;
   press L1 to go back. Try releasing Triangle while page 2 is active.
4. First verify existing owned items absent from page 1 appear on extra pages.
   Gain a *new* weapon with a full first wheel. Confirm it appears on page 2
   and persists after reload. Test already-owned pickups as well.
5. Test save/load, respawning, entering menus and swapping planets, and capture
   the log, especially any 'RAC2 HUD cache mismatch' warning.

**Next development milestone:** Add proper RAC2 wheel-update/draw and
pause-editor hooks to OpenRAC, then replace the per-frame projection with
scoped native hooks and add L2/R2 editor page switching.

## Dev.3: native paused wheel

Builds against SDK 1.34.0 (requires menu overlay API 1.31). The HUD text is
now drawn by the menu-capable overlay rather than gameplay-only hud_text_set.
An existing wheel session may continue in the menu phase when the native
saved mode is 7 (D_001BACC0, menu_is_state_seven). The selected extra page
remains active through the paused wheel's release animation. Other menu modes
are rejected; the log records the observed native mode on transition.
This does not yet supply dedicated native wheel hooks or guarantee all
selection timings. Validate mode relocation and page switching in game.

## Dev.4: use the port's Quick Select context

Dev.3's menu-saved-state check returned 0 in the live log. Replace it with the
same discriminator as OpenRAC's port/game/rac2/host/touch.c: g_nGameState
(0x001A8BB0) == 4 and g_gameStatePendingArgA (0x001AA0F8) == 7, using
mod.relocate() (the SDK equivalent of OPENRAC_DATA). Log the actual state
and argument, and recover a held-Triangle session in that confirmed menu.
The overlay API is unchanged. If save_slot_current() returns -1, extra
pages are available in memory but not written to a guessed save slot.

## Dev.5: resolve the live native radial cache

Dev.4 confirmed state=4 arg=7 but the page projection rejected a zeroed cache.
The NTSC-U addresses are executable addresses: relocate_level(loaded_level(),
address) is a no-op, not executable-to-level relocation. Use relocate() for
item records and radial globals, then follow g_radial_menu_pages (0x001B18D4)
to the actual page-zero cache as native wheel update/draw do. Verify eight
slots, page index zero, RAM bounds, canonical item IDs and icon values before
projection. Extra-page icon values must also be valid. Log requested page,
resolved cache/records on mismatch, successful validation, and inventory fill.
Still experimental; no pause-editor or save-slot fallback changes.

## Dev.6: decode owned inventory IDs

Dev.5 confirmed live cache validation and switching. The inventory order uses
six-bit IDs, not seven: help_store_id_in_slot_list (002883E8) adds 0x40 to
owned entries; level_validate_item_list (002908D0) masks 0x3F. For example,
0x5E is owned item 30, not item 94. Correct the mask, skip 0xFF, and still
require the separate owned flag. Filter missing icons and the 0xEA7E icon
rejected by the native ring editor. Test actual flagged list values and log
eligible IDs and first raw list if unavailable. No changes to RAC1, projection
or persistence.
