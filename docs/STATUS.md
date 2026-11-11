# Expanded Quick Select — rebuild checkpoint (2026-11-11)

This is an **experimental hook implementation**. The user confirmed the wheel works almost flawlessly in-game; the one reported issue is R1 switching pages during an airborne jump also causing a Thruster-Pack slam.

## Recovered and preserved decisions

- Mod ID `expanded-quick-select`, native C++17, RAC1 PAL and NTSC-U.
- Independent eight-slot pages with L1/R1 switching only during Triangle Quick Select.
- Page 1 stays in the normal save; page 2 is stored separately per save slot.
- Use the game's existing wheel/editor rendering; display a small page number.
- Coexist with but never modify the existing Weapon Wheel Pause mod.

## Built in the initial checkpoint

- Native C++ entry-point scaffold and source manifest.
- `Banks` model for both slot pages with edge-based button switching.
- `SlotStore` that validates file magic, version, size, items and checksum.
- Same-directory temporary save and atomic replacement on supported platforms.
- Dependency-free native unit tests for the model and storage.

## Verification performed here

- The `scripts/test-local.sh` core suite passes under GNU C++17 including `-DNDEBUG`.
- The manifest is syntactically valid JSON.
- GitHub Actions Windows x64 on the `development` branch compiled the native mod with SDK 1.14.5, ran the unit tests, and packaged a development ZIP. This is **build verification only**, not proof that the runtime layout works in-game.

## New live integration (untested in-game)

- Hooked HUD initialization, update and draw; L1/R1 page changes when Triangle is held.
- Temporary projection swaps the canonical 8 slots and HUD entry cache only within intercepted calls, guarded by matching slot IDs **and icon definitions**; restores both afterwards.
- Hooked pause editor load, update and store; L2/R2 switch editor pages to preserve the original L1/R1 cursor movement.
- Hooked save preparation, restore and new game, with second-page data in per-save-slot files.
- HUD page label through the 1.12+ API; Windows development-only Actions artifact workflow.

## Next engineering work

1. Run the Windows development workflow against the real OpenRAC 1.14.5 SDK, fix compilation or packaging errors.
2. Verify actual PAL and NTSC-U addresses, overlay relocation and HUD icon-table layout in-game. Stop projection if the guard fails.
3. Confirm editor hook argument ownership and L2/R2 switching, including how returning from submenus triggers Store.
4. Test memory-card saves, restore, New Game Plus, unsaved games and save-slot switches with a backup.
5. Add a page indicator to the pause menu itself and improve L1/R1 page navigation as the interface permits.
6. Only after successful runtime tests, consider a separately tagged beta release and other platform builds.

## Known design risks to resolve

- Game-memory addresses are internal and experimental; safe layout checks are required.
- L1/R1 may already have effects while Quick Select is open; the input hook must only consume them when appropriate.
- The `SlotStore` validates item IDs 0–39 and save-slot numbers 0–127; live card-slot numbering still needs verification.
- Overlay symbol addresses come from the PAL level-zero decomp and the NTSC -0x100 shift is **not yet verified** for all overlay symbols. The HUD table is checked before writes, but testing on a backed-up save is essential.
- Win32 `MoveFileExW` and POSIX rename are used for replacement; test actual Windows file replacement before trusting persistence on a player's save.

## v0.1.1: wheel-only shoulder input consumption

- A gameplay-scoped Hero_Update hook masks L1/R1 in the gameplay-facing held/pressed/raw button fields when Triangle is held. The weapon wheel still reads held_unmasked, so L1/R1 page navigation remains available.
- The original bits are restored after the hero update, avoiding changes to gameplay actions outside Quick Select.
- Source-level tests cover Triangle-on/off, both shoulders, and unrelated buttons.
- Runtime behaviour with and without Weapon Wheel Pause must be tested in-game; no claims are made about its correctness before this test.
