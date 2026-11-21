# Optional compatibility API v1

**Introduced in Expanded Quick Select v1.0.3.** EQS continues to work
independently and does not require any external mod.

- Header: `include/expanded_quick_select/compat_api.h`
- Export: `openrac_expanded_quick_select_query(1)`
- Discovery: resolve the symbol from the **already loaded** EQS plugin;
  never load a second copy just to obtain the API. On Windows, clients can
  use `GetModuleHandleW(L"expanded_quick_select.dll")` followed by
  `GetProcAddress`. Other platforms export the symbol too, but cross-mod
  discovery there is not yet tested.
- Thread: call from OpenRAC's game thread. Verify table size, version,
  pointers and each result before use.

The API table contains `get_state` and `get_slots`, both returning 1 on
success and 0 on failure. Before calling, set each output structure's
`size` to `sizeof(output)`.

`get_state` exposes `page_count` (1–8), zero-based `gameplay_page`
and `editor_page`, and flags for whether each screen is open. An inactive
page has value `EQS_COMPAT_NO_PAGE`. `get_slots(page)` returns a copy
of eight **native** item IDs for that configured page.

## Custom-weapon boundary

This read-only interface does **not** distinguish original Walloper
(native item 18) from Revolverator (logical key `0x10012`), which uses
the same native carrier. Never write a logical custom key into the game's
native favourites or item tables.

EQS keeps its original save scheme: page 1 in the game's save, pages 2–8
in EQS's existing sidecars. This API cannot assign or persist logical custom
weapons to secondary pages. A separate versioned binding protocol would be
needed to support that safely.

## HUD compatibility

v1.0.3 also allows cached icon 60018 and live icon 60037 (or the reverse)
**only** for native item 18. For every other item, the same strict HUD
validation still applies. This addresses a known Walloper/Revolverator
carrier-icon mismatch during page projection; it does not repair custom
HUD textures, post-respawn icons or another mod's ammunition handling.

## Integration and testing status

Revolverator has an experimental `eqs-compat-pr` integration that reads
this API. Windows NTSC-U tests have confirmed that independent EQS wheels
can retain their native assignments while Revolverator is equipped, but
Revolverator's icon may still be wrong after death and respawn. Its original
post-respawn ammo-capacity bug is independent and outside this release.

The EQS automated build covers Windows x86-64, Linux x86-64/aarch64,
macOS arm64/x86-64 and Android ARM64. Build success alone does not verify
EQS's new interop on every operating system or RAC1 PAL.
