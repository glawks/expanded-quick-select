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

## Editor compatibility status

The earlier experimental safeguard against bulk native-slot writes was
**removed** after live testing showed that it prevented normal secondary
wheel assignments with Revolverator installed. The editor now uses its
unchanged v1.0.2 update/capture behavior.

This branch does **not yet fix** Revolverator's overlapping eight-slot
registry, page duplication, custom icon rendering, or independent per-page
custom-weapon identity. Resolving those issues requires a deliberate
cross-mod contract; do not assume that the read-only API alone provides
compatibility.

The page-two legacy sidecar format and all other page files are unchanged.
If experimental testing has already saved an empty page, updating the plugin
cannot reconstruct those assignments. Restore backed-up EQS data if needed.

## Opt-in diagnostic logging

Enable the **Log Quick Select interoperability diagnostics** setting in the
development mod and restart. Every editor open, page change, or native slot
rewrite within the shared update hook can report the edited page, whether Cross
was pressed, and eight slot IDs before/after. Logging stops after 64 events.
This is **observational only**: the mod no longer blocks full-wheel writes.

For a suggested cooperative solution and exact source-level conflicts, see
[REVOLVERATOR-INTEGRATION.md](REVOLVERATOR-INTEGRATION.md).

## Validation before release

Test native-only editing, slot moves and removal, different game save slots,
page wrapping, save/load, and all six supported platform builds. With
Revolverator enabled, test both plugin load orders, assignment on page 1 and
page 2, display and equip, save/load, and weapon identity. Do not claim
compatibility until those in-game tests pass.
