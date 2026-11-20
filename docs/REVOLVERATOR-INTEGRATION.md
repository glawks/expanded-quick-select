# Revolverator × Expanded Quick Select — proposed compatibility contract

**Status: discussion / integration proposal, not implemented compatibility.**
This document records source-level findings for the two developers and defines
the decisions needed before either mod changes how it stores weapon identity.

## Reproduction

1. Install both mods. Configure at least two Expanded Quick Select wheels.
2. Assign different native weapons on pages 1 and 2.
3. Assign Revolverator in the pause-menu Quick Select editor.
4. Observe page 2+ copying page 1 or showing incorrect Revolverator icons.
5. A previous EQS-only "reject full-wheel rewrite" experiment made page 2+
   blank and prevented normal assignments. **That safeguard was reverted.**

Make a backup of both mods' save sidecars before trying a development build.
If an affected empty page was already persisted, a code change cannot restore it.

## Current code observations

- EQS owns up to eight pages of **native item IDs**. Page 1 is in the game's
  ordinary save and pages 2–8 in EQS sidecars. It swaps an active page into
  native HUD memory only temporarily, and captures the editor's eight native
  slots after the native `item_slots.update` callback.
- Revolverator owns its own **one-page logical registry**, `registry.quick()`,
  including `0x10012` for Revolverator. Its native carrier is Walloper (18).
- Revolverator's `NativeWeaponMenu::slots_update()` calls
  `write_native_slots(screen)` after an assignment; that method replaces all
  eight visible editor slots from its single-page logical registry.
- Its `quick_load()` and `quick_store()` also project that native registry;
  its gameplay `selected_quick()` and `quick_draw()` use the same one-page
  logical registry. A custom icon can therefore be drawn on multiple pages
  even when their native assignments are different.
- Existing Revolverator API v3 provides `get_quick()`, but no assignment
  setter or way to select which page its logical registry represents.
- Load order may change which hook sees which data first, but it cannot
  resolve two independent sources of truth for the same page.

## What EQS already exposes (available on compatibility-dev)

Header: `include/expanded_quick_select/compat_api.h`
Symbol: `openrac_expanded_quick_select_query(1)`

- `get_state()`: number of pages and zero-based gameplay/editor page indices,
  or `EQS_COMPAT_NO_PAGE` when a wheel/editor is inactive.
- `get_slots(page)`: eight **native** item IDs for a configured page.

The interface is copy-only, optional, game-thread-only, and has no write methods.
Do not try to infer custom identity from native ID 18.

## Minimal cooperative solution — preferred first experiment

The Revolverator developer may be able to use EQS's existing v1 API without
adding any write endpoint to EQS:

1. If EQS is absent, keep current single-wheel behavior unchanged.
2. If EQS is active, query the page before HUD draw, Quick Select selection,
   menu editing and other hooks that currently read `registry.quick()`.
3. In the editor on pages 2–8, do not replace the entire 8-slot array with
   `registry.native_slots()`. Keep unrelated EQS page assignments. When the
   user assigns Revolverator, update only the selected slot's *native carrier*
   and record its distinct logical identity for that page/slot.
4. Use a **per-page logical assignment** for the custom icon and for equip
   selection. Do not read one-page `registry.quick()` for every visible page.
5. Persist the custom logical assignment separately and reload it by save slot.
   Page 1 may continue using Revolverator's existing logical save ownership;
   pages 2–8 need extra identities. Native ID 18 alone is insufficient.
6. Avoid unconditional writing of `registry.native_slots()` over an EQS
   secondary page during HUD init, editor load/store and gameplay.
7. Handle hook chaining in either order and preserve normal behavior when
   only one of the mods is installed.

**Tradeoff:** Revolverator would own the page-specific logical metadata and
would need its own sidecar migration/versioning. This is the smallest change
to EQS, but other custom-weapon mods would need similar logic.

## Alternative: agreed shared logical-binding API (proposed v2)

If both developers prefer one shared representation, extend EQS's C ABI
*after agreement* rather than pretending native IDs carry logical identity.

Possible **proposed** v2 operations (names and arguments not final):

- `get_binding(page, slot)` returns a stable provider ID, 32-bit logical weapon
  key and native carrier; `get_slots(page)` remains native-only for v1 callers.
- `register_provider(provider_id, ...)` permits optional custom providers.
- `set_custom_binding(page, slot, ...)` is used after a real editor assignment
  and modifies only the requested page/slot, never an entire wheel.
- An active-page or selection notification ensures custom draw/equip hooks
  use the same page and slot. Events must be defined without relying on
  fragile mod load order.
- Versioned per-save sidecars persist logical keys with stable provider IDs,
  preserve existing EQS native-only sidecars, and distinguish Walloper from
  Revolverator even though both use native 18.

**Do not implement this contract blindly.** Registration, ownership,
unregistration, save compatibility and callback reentrancy must be agreed first.

## Recommended validation matrix

- EQS only, Revolverator only, both; PAL and NTSC.
- Different mod load orders; load, edit, close, reopen, change pages and select.
- Revolverator on page 1 only, page 2 only, and moving it between pages.
- Walloper and Revolverator in different slots/pages at the same time.
- Native-only assignment and removal on other pages with Revolverator present.
- Re-equipping across page switches, with each displayed icon matching the
  **actual** equipped weapon.
- Save/reload, save-slot changes, new game, hidden pages after reducing wheel
  count, old sidecar migration, and disabled/re-enabled Revolverator.
- Platform builds and Android input hints.

For reproduction logs, enable EQS's **Log Quick Select interoperability
diagnostics** setting on `compatibility-dev` and Revolverator's own **trace**
setting. EQS will log editor page, Cross state and eight native IDs before
and after the shared update hook (maximum 64 events per run). The EQS trace
never blocks or modifies assignments.

## Useful source locations

- EQS: `src/mod.cpp` — editor `Menu_ItemSlots_Update`, `Menu_QuickSelect_Store`;
  gameplay projection and save-slot lifecycle.
- Revolverator: `revolverator/src/native_weapon_menu.cpp` —
  `slots_update`, `write_native_slots`, `quick_load`, `quick_store`.
- Revolverator: `revolverator/src/mod.cpp` —
  `selected_quick`, `quick_init`, `quick_draw`, `quick_update`.
- Revolverator: `revolverator/src/presentation.cpp` —
  `draw_quick_selector` using `registry.quick()`.
- Revolverator: `revolverator/include/revolverator_api.h` — existing API v3.
