# Publishing a new Expanded Quick Select release

This repository builds native OpenRAC mod libraries for Windows x86-64,
Linux x86-64/aarch64, and macOS Intel/Apple Silicon. Every build produces
individual platform ZIPs plus one combined installable ZIP and SHA256SUMS.

## Release checklist

1. Finish and test changes on the `development` branch.
2. Set the **same version** in:
   - `openrac-mod.json` — `"version"`
   - `include/expanded_quick_select/identity.hpp` — `kModVersion`
   - `tests/test_smoke.cpp` — expected version in the smoke test.
3. Check README, MIT `LICENSE`, manifest and compatibility notes.
4. Run the development CI workflow. Verify all five builds, core tests,
   the merged ZIP, and SHA256SUMS pass.
5. Merge `development` into **`main`** and ensure main's CI passes.
6. Tag the tested **main** commit. For the first full release:

   ```sh
   git switch main
   git pull --ff-only
   git tag v1.0.0
   git push origin v1.0.0
   ```

   Do **not** push `v1.0.0` while the manifest still says `0.2.0`;
   the publishing workflow will reject any tag/version mismatch.
7. Open **GitHub → Actions → Cross-platform build and release**. On the tag
   run, check that the five builds and merged package validation pass and
   that **Publish verified GitHub Release** succeeds.
8. Open GitHub Releases and verify the release has:
   - `expanded-quick-select-VERSION.zip` (combined installer)
   - Five `expanded-quick-select-VERSION-PLATFORM.zip` archives
   - `SHA256SUMS` listing all six ZIPs.

The release workflow starts on a pushed `vX.Y.Z` or `vX.Y.Z-suffix` tag.
Normal pushes and manual workflow runs **only** produce Actions artifacts.
A release requires a successful merge job, matching tag/manifest versions,
a combined ZIP containing all five platform libraries, the expected MIT
licence, and matching SHA-256 hashes. Release notes are generated automatically.
Prerelease suffixes (for example `v1.0.0-rc.1`) are marked as prereleases.

Do **not** upload or replace release ZIPs manually: OpenRAC validates archives
against the release's `SHA256SUMS`. If CI fails, fix the source on
`development` and prepare a new version/tag rather than reusing a published
tag from a different commit.

## Platform test status

Windows NTSC-U is tested in-game. Linux/macOS builds pass automated checks
but still need gameplay testing, as does the RAC1 PAL game version.
Do not describe compilation alone as confirmed gameplay compatibility.

## Visibility

This repository is currently private. A GitHub Release on a private
repository is **not publicly available**. Make the repository public when
you're ready for players to download or install the mod via its URL.
