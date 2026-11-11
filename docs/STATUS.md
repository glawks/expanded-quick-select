# Expanded Quick Select — rebuild checkpoint (2026-11-11)

This is a **source-only development checkpoint, not a playable mod**.

## Recovered and preserved decisions

- Mod ID `expanded-quick-select`, native C++17, RAC1 PAL and NTSC-U.
- Independent eight-slot pages with L1/R1 switching only during Triangle Quick Select.
- Page 1 stays in the normal save; page 2 is stored separately per save slot.
- Use the game's existing wheel/editor rendering; display a small page number.
- Coexist with but never modify the existing Weapon Wheel Pause mod.

## Built in this checkpoint

- Native C++ entry-point scaffold and source manifest (no wheel hooks yet).
- `Banks` model for both slot pages with edge-based button switching.
- `SlotStore` that validates file magic, version, size, items and checksum.
- Same-directory temporary save and atomic replacement on supported platforms.
- Dependency-free native unit tests for the model and storage.

## Verification performed here

- The `scripts/test-local.sh` core suite passes under GNU C++17 including `-DNDEBUG`.
- The manifest is syntactically valid JSON.
- No OpenRAC SDK or CMake is installed in this workspace, so the OpenRAC plugin itself has **not** been compiled, loaded, or tested in-game.

## Next engineering work

1. Obtain the OpenRAC 1.11+ SDK and official native template, including its exact header declarations, fetch/lock tooling and build workflow.
2. Locate and verify PAL/NTSC-U addresses/layouts for RAC1's canonical eight Quick Select slots, pause-menu editor slots and wheel-open flag. Do not guess addresses.
3. Build a transactional slot projection around the official HUD `init/update/draw` hook calls; restore primary assignments on all exits, including save hooks.
4. Implement the separate two-page pause-menu editor using official `load/store/draw` hooks, with L1/R1 input arbitration.
5. Wire `save_slot_current()` and new-game/save/restore hooks to the `SlotStore`, covering unsaved games and slot changes.
6. Compile for Windows x64 first, test in-game on a copy of a save, then verify other platform builds and package a test-only release.

## Known design risks to resolve

- Game-memory addresses are internal and experimental; safe layout checks are required.
- L1/R1 may already have effects while Quick Select is open; the input hook must only consume them when appropriate.
- The `SlotStore` currently conservatively allows item IDs 0–255 and save-slot numbers 0–127. Those bounds must be checked against the actual game/SDK before connecting to in-game data.
- Win32 `MoveFileExW` and POSIX rename are used for replacement; test actual Windows file replacement before trusting persistence on a player's save.
