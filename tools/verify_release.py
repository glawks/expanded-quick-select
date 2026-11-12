#!/usr/bin/env python3
"""Check a version-tag release package (also exercised on development CI).

No GitHub API or credentials. Must pass before the release job uploads files.
"""
import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

PLATFORMS = (
    "windows-x86_64",
    "linux-x86_64",
    "linux-aarch64",
    "macos-arm64",
    "macos-x86_64",
)
MOD_ID = "expanded-quick-select"


def verify(tag: str, dist: Path, root: Path) -> None:
    manifest = json.loads((root / "openrac-mod.json").read_text(encoding="utf-8"))
    version = manifest["version"]
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z][0-9A-Za-z.-]*)?", tag):
        raise ValueError(f"Invalid version tag {tag!r}: use vX.Y.Z or vX.Y.Z-suffix")
    if tag != "v" + version:
        raise ValueError(f"Tag {tag!r} does not match openrac-mod.json version {version!r}")
    if manifest.get("license") != "MIT":
        raise ValueError("Expected MIT licence in openrac-mod.json")
    if manifest.get("id") != MOD_ID:
        raise ValueError(f"Unexpected mod ID: {manifest.get('id')!r}")
    # The SDK packages the JSON independently of the DLL/SO/DYLIB version
    # reported by OPENRAC_MOD_CPP; both must agree or OpenRAC won't load it.
    identity = (root / "include/expanded_quick_select/identity.hpp").read_text(
        encoding="utf-8"
    )
    declared = re.search(r'kModVersion\[\]\s*=\s*"([^"]+)"', identity)
    if not declared or declared.group(1) != version:
        raise ValueError(f"Compiled mod version {declared.group(1) if declared else None!r} "
                         f"does not match manifest version {version!r}")
    if not (root / "LICENSE").is_file():
        raise ValueError("Root LICENSE file missing")

    combined = f"{MOD_ID}-{version}.zip"
    names = {combined}
    names.update(f"{MOD_ID}-{version}-{platform}.zip" for platform in PLATFORMS)
    sums_path = dist / "SHA256SUMS"
    if not sums_path.is_file():
        raise ValueError("SHA256SUMS missing")
    checksums = {}
    for line in sums_path.read_text(encoding="utf-8").splitlines():
        digest, separator, filename = line.partition("  ")
        if not separator or not re.fullmatch(r"[a-f0-9]{64}", digest):
            raise ValueError(f"Malformed SHA256SUMS entry: {line!r}")
        if filename in checksums:
            raise ValueError(f"Repeated checksum for {filename}")
        checksums[filename] = digest
    if set(checksums) != names:
        raise ValueError(f"Unexpected checksum manifest: missing={sorted(names - set(checksums))}, "
                         f"extra={sorted(set(checksums) - names)}")
    for name in sorted(names):
        archive = dist / name
        if not archive.is_file():
            raise ValueError(f"Release asset missing: {name}")
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if checksums[name] != digest:
            raise ValueError(f"SHA256 mismatch for {name}")

    with zipfile.ZipFile(dist / combined) as archive:
        if archive.testzip() is not None:
            raise ValueError("Combined ZIP has a corrupted member")
        files = set(archive.namelist())
        contents = json.loads(archive.read("openrac-mod.json"))
        for key in ("id", "version", "license", "api", "games", "settings", "hooks"):
            if contents.get(key) != manifest.get(key):
                raise ValueError(f"Combined ZIP manifest mismatch: {key}")
        plugins = contents.get("plugin", {})
        if set(plugins) != set(PLATFORMS):
            raise ValueError(f"Wrong bundled platforms: {sorted(plugins)}")
        for platform, path in plugins.items():
            if path not in files or not path.startswith(f"lib/{platform}/"):
                raise ValueError(f"Missing or misplaced {platform} library: {path}")
        for filename in ("README.md", "LICENSE"):
            if filename not in files:
                raise ValueError(f"Missing {filename} in combined ZIP")
            if archive.read(filename) != (root / filename).read_bytes():
                raise ValueError(f"Combined ZIP {filename} differs from repository")
    print(f"Release verification passed: {tag}, {len(PLATFORMS)} platforms, "
          f"{len(names)} archives and SHA256SUMS")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--dist", type=Path, default=Path("dist"))
    args = parser.parse_args()
    try:
        verify(args.tag, args.dist, Path("."))
    except (ValueError, KeyError, OSError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        print(f"::error::{exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
