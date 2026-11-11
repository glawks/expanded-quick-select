#!/usr/bin/env bash
set -euo pipefail

# Core-only fallback for development systems without CMake or the Mod SDK.
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf -- "$build_dir"' EXIT

compiler="${CXX:-g++}"
flags=(-std=c++17 -O2 -DNDEBUG -Wall -Wextra -Werror -I"$repo_dir/include")
"$compiler" "${flags[@]}" "$repo_dir/tests/test_smoke.cpp" -o "$build_dir/smoke"
"$compiler" "${flags[@]}" "$repo_dir/tests/test_banks.cpp" "$repo_dir/src/banks.cpp" -o "$build_dir/banks"
"$compiler" "${flags[@]}" "$repo_dir/tests/test_slot_store.cpp" "$repo_dir/src/slot_store.cpp" "$repo_dir/src/banks.cpp" -o "$build_dir/slot_store"

for test in smoke banks slot_store; do
    "$build_dir/$test"
    printf 'PASS %s\n' "$test"
done
