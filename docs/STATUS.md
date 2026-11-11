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


## v0.1.2: airborne slam regression
- In-game report confirmed v0.1.1 suppressed ordinary ground crouch but airborne
  Thruster-Pack slam still triggered alongside Quick Select page navigation.
- Confirmed RAC1's `process_pad_input` appends `pressed` to PadState's
  `hist_btn[30]` ring, and `pad_pressed_within` scans this ring to recognise
  recent presses, bypassing the v0.1.1 current-state-only filter.
- A new `Pad_ProcessReport` hook calls the original input processor first, then,
  while Triangle is held on the primary pad, removes L1/R1 from the newest
  recorded press and gameplay-facing held/pressed/raw fields, leaving
  held_unmasked/pressed_unmasked intact for the weapon-wheel mod.
- The v0.1.1 Hero_Update scoped filter remains as a secondary safeguard.
- New unit assertions cover wrap-around of the last history index and removal
  of R1 without removing unrelated button presses.
- In-game verification is still required; code build alone cannot prove this
  fixes the slam or every pad configuration.

## v0.1.3 — native pause editor hint (untested)

- Hook `rac1.menu.quick_select.draw` to draw a footer after the game's ring.
- Hook `rac1.menu.item_text.draw` to reuse the game's own small menu font.
- Temporary backed-up screen and help-message table fields are restored after the callback. HUD text is not used because OpenRAC draws it behind menus.
- Bottom line: `L2   PAGE 1 / 2   R2` / `L2   PAGE 2 / 2   R2`, reflecting the editor's active page.
- Native text setup, placement and message table lookup still need in-game verification on PAL and NTSC-U. v0.1.2 remains the last verified gameplay build.

## v0.1.4 — page hint visible-state diagnostics

- User confirmed v0.1.3 loads but no editor footer appears (NTSC-U).
- Corrected the overly restrictive help-message ID filter (first 32 entries with IDs 0x4000..0x5FFF); now select the first plausible message record in the active language table. The `help_msgs` global is indeed a pointer, not an inline table.
- The footer logs each stage once or on state changes (editor opened, native font callback seen, missing font, message-table/record issue, viewport dimensions or attempted native draw).
- No changes to existing gameplay selection, jump input or save logic. Still awaiting in-game rendering check.

## v0.1.5 — NTSC-U message-table address fix (not yet verified in-game)

- User's v0.1.4 log: `Quick Select footer: editor opened` and `message table invalid: ptr=0 count=1515`.
- Verified NTSC-U's `get_help_message_text` reads pointer at `D_0015F6A0`; the -0x100 PAL adjustment read `0x0015F680`, which is not that pointer.
- Use explicit PAL `0x0015F780` / NTSC `0x0015F6A0` address selector and preserve `mod.relocate` semantics.
- No longer borrow an unverified HUD text buffer. During a draw, temporarily replace a sufficiently long existing help message with the footer and restore its original bytes immediately afterward.
- Gameplay wheel, pad history fix, saves and page changing unchanged.
- Logs now include message pointer slot, pointer and count if lookup fails, or a native draw attempt.
