# Expanded Quick Select (development)

A standalone Ratchet & Clank 1 OpenRAC mod intended to provide two independently editable eight-slot Quick Select pages, switched using L1/R1 while holding Triangle.

**Not installable yet.** This repository is a fresh rebuild. The current native entry point is a scaffold only and deliberately registers no game hooks. It does not yet change the weapon wheel.

The existing Weapon Wheel Pause mod is not part of this repository and remains untouched.

## Build environment

The native build requires the official OpenRAC Mod SDK (1.11 or newer), CMake 3.20+, and a C++17 compiler. Configure CMake so it can find `OpenRACMod` in the SDK's CMake package directory. Use the official OpenRAC native-mod template's SDK-fetching workflow when packaging a release.

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_OPENRAC_MOD=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Enable `-DBUILD_OPENRAC_MOD=ON` when the SDK is available. Do not install or release this scaffold as a functioning mod.

Without CMake, the dependency-free logic tests can also be run on Linux/macOS with `bash scripts/test-local.sh`.

## Development status

- [x] Project identity, basic manifest and native load entry point
- [x] Pure C++ two-page assignment model and debounced input behaviour
- [x] Versioned, validated, per-save-slot secondary-page storage (core only)
- [ ] Live in-game wheel and editor hooks
- [ ] Save lifecycle and in-game testing
- [ ] Official SDK CI/release packaging
