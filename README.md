# Expanded Quick Select (development)

A standalone Ratchet & Clank 1 OpenRAC mod intended to provide two independently editable eight-slot Quick Select pages, switched using L1/R1 while holding Triangle.

**Experimental development build.** The wheel and second-page editor have been successfully tried in-game; v0.1.1 did not prevent the airborne R1 slam, so v0.1.2 now filters the pad event history and still awaits player verification. Make a backup of your save before testing, and expect possible missing icons, UI glitches or crashes. This is not a stable launcher release.

The existing Weapon Wheel Pause mod is not part of this repository and remains untouched.

## Build environment

The native build requires the official OpenRAC Mod SDK (1.14.5 pinned here), CMake 3.20+, and a C++17 compiler. The development workflow on GitHub Actions builds and tests a **Windows x64 artifact** on pushes to `development`. It does not publish a release.

### Testing controls

- Gameplay: hold **Triangle** to open Quick Select; press **L1 or R1** to switch between eight-slot pages. The wheel starts on page 1 each time it opens. In v0.1.2, shoulder presses while Triangle is held are consumed before world input handling, including the pad's recent-press history, rather than being interpreted as a gameplay attack or Thruster-Pack action.
- Pause-menu Quick Select editor: **L2/R2** switch pages. **L1/R1** keep their original slot-selection behaviour, and **Cross** assigns an item to the visible page.
- Page 1 remains stored by the game. Page 2 uses a separate file under OpenRAC's mod data folder, one for each memory-card save slot.
- The HUD displays `PAGE 1 / 2` or `PAGE 2 / 2` while Triangle is held. The pause editor does not yet show a page-number label.

To test: open the repository's **Actions → Windows development build** run, download the `expanded-quick-select-windows-test` artifact, extract the mod ZIP and install that ZIP through OpenRAC's Mods tab. No stable release is published.

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_OPENRAC_MOD=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Fetch the SDK with `python tools/fetch_sdk.py` and configure with `-DBUILD_OPENRAC_MOD=ON` for the real native mod. The fetch script verifies the SDK archive against its published SHA-256 file.

Without CMake, the dependency-free logic tests can also be run on Linux/macOS with `bash scripts/test-local.sh`.

## Development status

- [x] Project identity, basic manifest and native load entry point
- [x] Pure C++ two-page assignment model and debounced input behaviour
- [x] Versioned, validated, per-save-slot secondary-page storage (core only)
- [x] Development hooks for the gameplay wheel and pause-menu editor
- [x] Save/restore entry points and guarded secondary-page persistence
- [x] SDK compilation and initial in-game wheel/editor test
- [ ] Verify v0.1.2 early input/history suppression while jumping with the wheel open
- [x] Windows development artifact workflow (no public release)
