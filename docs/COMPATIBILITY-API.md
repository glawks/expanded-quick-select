# Optional compatibility API (development branch)

Expanded Quick Select exposes a **read-only C ABI v1** so another native mod can
identify the active wheel without guessing from guest memory or depending on
Expanded Quick Select being installed.

The header is `include/expanded_quick_select/compat_api.h`. The exported
query symbol is `openrac_expanded_quick_select_query`. Resolve it from the
**already loaded** EQS plugin, request version 1, and check the returned
table's `size`, `version` and function pointers. Never load another copy
of the mod solely to obtain this symbol. Use it only on OpenRAC's game thread.

Both page indices are **zero based** (page 1 is 0). The state contains:

- `page_count`: configured number of wheel pages (1–8)
- `gameplay_page`: wheel currently displayed during Triangle, or `EQS_COMPAT_NO_PAGE`
- `editor_page`: currently edited page, or `EQS_COMPAT_NO_PAGE`
- `flags`: `EQS_COMPAT_GAMEPLAY_OPEN` and/or `EQS_COMPAT_EDITOR_OPEN`

`get_slots(page, output)` reads a copy of the eight **native slot IDs** for
a configured page. Each caller-provided output structure must have `size`
set to `sizeof(output)`. Methods return 1 for success and 0 otherwise.
The API table is owned by EQS; never retain it after EQS unloads. This is an
optional feature: without EQS, consumers keep their ordinary behavior.

## Important boundary: custom weapons

This version deliberately **does not** claim to solve custom weapon
assignment. Native slot ID 18 is Walloper's ID, also used as a carrier by
Revolverator; the native ID cannot tell those logical weapons apart. EQS's
existing save files store native IDs only, while Revolverator's logical key is
`0x10012`. Never write `0x10012` to an in-game native item table.

A future API revision can add logical-identity registration, per-page binding
operations, custom preview/icon integration and save coordination. Those
capabilities require agreement with the custom-weapon developer and a new
backward-compatible persistence strategy. Do not reinterpret version 1's
`native_ids` as logical keys.

## Defensive editor handling

In this branch, **secondary pages only** validate modifications after the
native slot-update chain. Without a Cross assignment, changes are rejected;
with Cross, more than two changed slots are rejected and the previously
selected page is restored. This is a guard against bulk rewrites such as a
plugin restoring its original eight-slot registry. It does **not** intercept
all possible hook orders or provide full Revolverator compatibility.

The normal page-one Quick Select is not subject to the secondary-page guard.
The page-two legacy sidecar format and all other page files are unchanged.

## Validation before release

Test native-only editing, slot moves and removal, different game save slots,
page wrapping, save/load, and all six supported platform builds. With
Revolverator enabled, test both plugin load orders, assignment on page 1 and
page 2, display and equip, save/load, and weapon identity. Do not claim
compatibility until those in-game tests pass.
