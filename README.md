# Expanded Quick Select

**More weapon slots for Ratchet & Clank 1.** Expanded Quick Select is a standalone [OpenRAC](https://github.com/OpenRAC/OpenRAC) mod that adds up to **eight independently configurable Quick Select wheels**, with eight slots per wheel.

The number of wheels is adjustable in the OpenRAC launcher, with **two wheels enabled by default**. Switch between them using the shoulder buttons, or configure your weapons from the game's existing pause-menu editor. Your assignments are saved separately for each game save slot.

## Features

- **1–8 weapon wheels** (up to 64 Quick Select slots), selectable in the mod's launcher settings.
- **L1/R1 page switching** while holding Triangle; left moves back and right moves forward.
- **L2/R2 page switching** in the pause-menu Quick Select editor, without changing its normal slot-selection controls.
- **Page indicators** in the gameplay wheel and pause-menu editor, showing `PAGE X / Y`.
- **Persistent assignments** for each save slot, including pages beyond the original eight slots.
- **Wrap-around navigation**, so moving past the first or last page cycles to the other end.
- **Input protection** to prevent Quick Select shoulder-button navigation from triggering an unwanted airborne slam.
- **Standalone operation**; no dependency on the separate Weapon Wheel Pause mod.

## Installation

You need **OpenRAC with Ratchet & Clank 1** (PAL or NTSC-U) and a build supporting the OpenRAC native mod API **1.12 or later**.

1. Download the **combined mod ZIP** from this repository's [GitHub Releases](https://github.com/glawks/expanded-quick-select/releases) once a release has been published.
2. Open the **OpenRAC Launcher → Mods** tab.
3. Add or install the downloaded mod ZIP and enable **Expanded Quick Select**.
4. Start Ratchet & Clank 1.

When a release is available, the launcher can also install the mod from the repository URL:

```text
https://github.com/glawks/expanded-quick-select
```

The combined ZIP contains Windows, Linux and macOS native libraries; OpenRAC chooses the correct one for the user's platform.

## Configuration

In the OpenRAC launcher, open **Mods → Expanded Quick Select → Weapon wheels** and choose a number from **1 to 8**. The default is **2**.

**Restart the game after changing this setting.** Choosing one wheel leaves you with the original eight-slot layout and disables page switching. Increasing the count makes the additional pages available in the pause-menu editor.

### Controls

| Action | Input |
| --- | --- |
| Open Quick Select during gameplay | Hold **Triangle** |
| Previous / next wheel in gameplay | **L1 / R1** while Triangle is held |
| Previous / next wheel in the pause-menu editor | **L2 / R2** |
| Navigate weapon slots in the pause-menu editor | **L1 / R1**, as in the original game |
| Assign the selected weapon in the editor | **Cross**, as in the original game |

The gameplay wheel always starts on **page 1** when opened. For example, with four pages enabled, pressing R1 cycles **1 → 2 → 3 → 4 → 1**; L1 moves in reverse.

The editor remembers changes when you move between pages, and its label shows the currently selected page, such as `L2   PAGE 3 / 4   R2`.

## Saving and existing saves

**Existing RAC1 saves remain compatible.** The mod doesn't require a new game.

- **Page 1** continues to use the game's normal Quick Select assignments and save data.
- **Pages 2–8** are stored separately in OpenRAC's persistent mod-data folder, associated with each game save slot. They are not written into RAC1's normal memory-card save.
- The existing page-2 file format is preserved when upgrading from an earlier two-wheel version of Expanded Quick Select.

Lowering your configured wheel count **does not erase hidden pages**. For instance, if you configure six wheels, then reduce the setting to two, your assignments on pages 3–6 remain available when you increase the count again.

You can also load and save while the mod is disabled, then enable it again; the mod's extra-wheel data remains stored separately. **Do not delete the mod's data folder** if you want to keep those assignments.

## Compatibility

| Platform | Status |
| --- | --- |
| **Windows x86-64 — RAC1 NTSC-U** | **Tested in-game**, including multiple wheels, save persistence and page switching |
| **Windows — RAC1 PAL** | Supported by the mod; in-game testing still needed |
| **Linux x86-64 / ARM64** | Both native builds pass CI; in-game testing still needed |
| **macOS Apple Silicon / Intel** | Both native builds pass CI; in-game testing still needed |

The mod has also been tested alongside the separate **Weapon Wheel Pause** mod. Expanded Quick Select does **not** require it and does not bundle or alter its code.

Cross-platform **build success does not guarantee in-game compatibility**. Please report any issues with your operating system, RAC1 game version, and OpenRAC log.

## Troubleshooting

**Extra pages are missing:** Check the **Weapon wheels** launcher setting and restart the game after changing it.

**A page looks empty:** Open the pause-menu Quick Select editor, use **L2/R2** to select that page, and assign weapons to its slots.

**Assignments aren't being remembered:** Check that you loaded the same game save slot and that OpenRAC can write to the mod's persistent data folder. Don't remove the mod data when reinstalling.

**Crashes, incorrect icons or controls behaving unexpectedly:** Include the OpenRAC log, your operating system, PAL/NTSC-U version, and any other active mods in a GitHub issue.

## License

Expanded Quick Select's original code is released under the [MIT License](LICENSE).
Copyright (c) 2026 glawks.

Files originating from the OpenRAC Mod SDK (such as `tools/fetch_sdk.py`)
retain their original OpenRAC contributor copyright and licence notices.

## Building from source

This project is written in **C++17** and uses the official OpenRAC Mod SDK. The repository pins the SDK version in `sdk.lock` and provides a GitHub Actions workflow for Windows x86-64, Linux x86-64/ARM64, and macOS ARM64/Intel.

To run the portable core tests locally (CMake 3.20+ and a C++17 compiler):

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_OPENRAC_MOD=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

To build the native mod, fetch the SDK with `python tools/fetch_sdk.py`, then configure with `-DBUILD_OPENRAC_MOD=ON`. On Linux/macOS, `bash scripts/test-local.sh` also runs the dependency-free core tests.

Development builds and cross-platform packages are available as artifacts from the repository's **Actions** tab. Those are test builds, not official GitHub Releases.
