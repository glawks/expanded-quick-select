# Expanded Quick Select (development)

A standalone OpenRAC mod for **Ratchet & Clank 1**, adding independently
editable Quick Select pages. Choose **1 to 8 weapon wheels** in the OpenRAC
launcher's **Mods → Expanded Quick Select → Weapon wheels** setting.
**Two wheels is the default**, preserving the behaviour of v0.1.6.
Settings take effect the next time the game starts.

## Controls

| Action | Controls |
|---|---|
| Open the weapon wheel | Hold Triangle |
| Move to previous / next wheel | L1 / R1 while Triangle is held |
| Open Quick Select editor | The game's normal pause menu |
| Previous / next editor page | L2 / R2 |
| Move to a slot in the editor | L1 / R1 (vanilla controls) |
| Assign a weapon | Cross (vanilla controls) |

Page navigation wraps at either end. For example, with 4 wheels, R1 cycles
1 → 2 → 3 → 4 → 1, while L1 reverses that order.
The first page is shown whenever a gameplay wheel opens.
Both the gameplay HUD and pause editor display `PAGE N / TOTAL`;
the editor also shows `L2` and `R2` hints along the bottom of its right panel.
When only one wheel is selected, page switching does nothing.

### Save compatibility

RAC1 continues to own **page 1** in its normal save file. All other pages are
stored by the mod under OpenRAC's per-mod `data_dir()`, separately for
each memory-card save slot. The existing v0.1.6 **page 2** data file
(`save-slot-N.dat`) is unchanged and readable without conversion.
Pages 3–8 use `save-slot-N-page-3.dat` and so on.

Changing the wheel count **does not delete extra-page saves**. If you switch
from 6 wheels to 2, save, then switch back to 6, the old assignments on
pages 3–6 should still be present. Avoid deleting the mod's data directory.
Saves made with the mod disabled do not contain these extra-page assignments,
but the mod's separate data files remain available on re-enabling it.

### Supported systems and release status

The mod targets RAC1 NTSC-U and PAL. **v0.2.0 has been verified in-game on
Windows NTSC-U**, including 1–8 wheel selection, page navigation, saved
assignments and compatibility with the separate Weapon Wheel Pause mod.
PAL remains unverified. Builds for Linux and macOS are now produced by CI,
but require in-game testing on those operating systems. Back up your saves
before testing a development build.

This is a separate mod; [Weapon Wheel Pause](https://github.com/glawks/Pause-Weapon-Wheel)
is not modified by this repository.

## Build / download

The private `development` branch runs a GitHub Actions **cross-platform
development build** using the pinned OpenRAC Mod SDK. It builds Windows x64,
Linux x64/ARM64, and macOS ARM64/Intel (Intel cross-compiled on ARM64), then
packages the results. In the latest successful workflow run, download the
**`expanded-quick-select-all-platforms-test`** artifact and install the
**`expanded-quick-select-0.2.0.zip`** inside it through the OpenRAC launcher.
Separate per-platform artifacts are also available for debugging.

The combined ZIP contains the platform-specific `.dll`, `.so`, and `.dylib`
libraries. OpenRAC selects the appropriate library automatically. CI checks
that all five declared platforms made it into the archive. Build success does
not prove the mod works in-game on every platform. **No GitHub Release or tag
is created by this development workflow.**

For local core tests (CMake 3.20+ and C++17 compiler):

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_OPENRAC_MOD=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

For the native mod, fetch the SDK with `python tools/fetch_sdk.py`
and use `-DBUILD_OPENRAC_MOD=ON`. The `sdk.lock` pins the SDK.
Without CMake, run `bash scripts/test-local.sh` (Linux/macOS with g++).
