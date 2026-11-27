#include "expanded_quick_select/identity.hpp"
#include "expanded_quick_select/rac2.hpp"
#include "expanded_quick_select/editor_hint.hpp"
#include "expanded_quick_select/gameplay_hint.hpp"
#include "expanded_quick_select/input_filter.hpp"
#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/slot_store.hpp"
#include "expanded_quick_select/compat_api.h"
#include "expanded_quick_select/wheel_bridge.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "expanded_quick_select/hud_icon_compat.hpp"
#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace {
namespace eqs = expanded_quick_select;

// PAL executable addresses and level-zero overlay data from OpenRAC/rac1-decomp.
constexpr openrac::gaddr kSavedSlots = 0x00141FA0;
constexpr openrac::gaddr kPadState = 0x0013CA40;
constexpr openrac::gaddr kHudEntries = 0x0017DBC0;
constexpr openrac::gaddr kItemDefs = 0x00179BC0;
// PAL's help state count; the current runtime already reads this correctly.
// The message-table POINTER has its own NTSC-U location; unlike many other
// fields it cannot be obtained with the game's generic -0x100 shift.
constexpr openrac::gaddr kHelpState = 0x001997D0;
constexpr std::uint32_t kTriangle = 0x10, kL1 = 0x04, kR1 = 0x08;
constexpr std::uint32_t kL2 = 0x01, kR2 = 0x02;

struct HudEntry {
    std::int32_t icon;
    std::uint8_t unused[20];
    std::int32_t item;
};
static_assert(sizeof(HudEntry) == 28, "RAC1 HUD entry layout changed");

struct Runtime {
    explicit Runtime(openrac::Mod& m)
        : mod(m), store(m.data_dir()), shift(m.version() == "ntsc" ? -0x100 : 0) {
        const auto count = m.setting<std::string>("wheel_count");
        // The launcher validates choice values; retain an older-mod
        // fallback in case of a manually edited settings file.
        banks.set_page_count(count.size() == 1 && count[0] >= '1' && count[0] <= '8'
                                 ? static_cast<std::size_t>(count[0] - '0') : 2);
    }
    openrac::Mod& mod;
    eqs::SlotStore store;
    eqs::Banks banks;
    eqs::WheelSession wheel;
    eqs::EditorSession editor;
    int shift;
    int last_save_slot = -2;
    openrac::gaddr editor_address = 0;
    bool warned_layout = false;
    bool label_visible = false;
    bool external_wheel = false;
    // Acquired from the game's own menu-label hook when it runs.
    openrac::rac1::Menu_ItemText_Draw::fn menu_text_draw = nullptr;
    std::string footer_diagnostic;

    void footer_note(const std::string& message) {
        if (footer_diagnostic == message) return;
        footer_diagnostic = message;
        mod.info("Quick Select footer: " + message);
    }

    openrac::gaddr exe(openrac::gaddr pal) const {
        return mod.relocate(static_cast<openrac::gaddr>(pal + shift));
    }
    openrac::gaddr overlay(openrac::gaddr level0) const {
        return mod.relocate_level(0, static_cast<openrac::gaddr>(level0 + shift));
    }
    eqs::Slots read(openrac::gaddr address) const {
        return mod.memory().read<eqs::Slots>(address);
    }
    void write(openrac::gaddr address, const eqs::Slots& value) const {
        mod.memory().write(address, value);
    }
    void refresh_primary() {
        const auto value = read(exe(kSavedSlots));
        if (eqs::valid_game_slots(value)) banks.set_primary(value);
    }
    void load_slot() {
        const int slot = mod.save_slot_current();
        if (last_save_slot == slot) return;
        last_save_slot = slot;
        for (std::size_t page = 1; page < eqs::kMaxPages; ++page) {
            const auto p = eqs::page_at(page);
            banks.set_page(p, store.load(slot, p).value_or(eqs::Slots{}));
        }
        refresh_primary();
    }
    void save_extra_pages() {
        const int slot = mod.save_slot_current();
        if (slot >= 0) {
            // Never overwrite hidden pages when the player temporarily
            // reduces the setting from (say) 6 wheels to 2.
            for (std::size_t page = 1; page < banks.page_count(); ++page) {
                const auto p = eqs::page_at(page);
                if (!store.save(slot, p, banks.slots(p)))
                    mod.warn("Could not persist page " + std::to_string(page + 1) +
                             " for save " + std::to_string(slot));
            }
        }
        last_save_slot = slot;
    }
    void label(bool visible) {
        if (!visible && !label_visible) return;
        label_visible = visible;
        if (!visible) { mod.hud_text_set({}); return; }
        openrac_hud_text text{};
        // Show discoverable L1/R1 navigation hints only if another wheel exists.
        const std::string message =
            eqs::gameplay_page_hint(banks.active_page(), banks.page_count());
        if (message.size() + 1 > sizeof(text.text)) return;
        std::memcpy(text.text, message.c_str(), message.size() + 1);
        text.x = 0.5f;
        text.y = 0.81f;
        text.scale = 0.62f;
        text.rgba[0] = text.rgba[1] = text.rgba[2] = 220;
        text.rgba[3] = 255;
        text.flags = OPENRAC_HUD_SMALL | OPENRAC_HUD_SHADOW;
        mod.hud_text_set({text});
    }
    void reset(bool new_game) {
        editor.end();
        external_wheel = false;
        // Clear the on-screen page triggers when a save is restored or reset.
        mod.touch_hint("eqs_pages", false);
        editor_address = 0;
        banks.reset_for_new_game(read(exe(kSavedSlots)));
        last_save_slot = -2;
        if (!new_game) load_slot();
        label(false);
    }
};

// Projection is scoped to the original game's HUD call. No second-page item
// is ever left in the canonical eight slots that RAC1 writes to memory cards.
class Projection {
public:
    explicit Projection(Runtime& r) : state(r) {
        if (r.banks.active_page() == eqs::Page::Primary) return;
        const auto second = r.banks.slots(r.banks.active_page());
        if (!eqs::valid_game_slots(second)) return;
        slots_address = r.exe(kSavedSlots);
        cache_address = r.overlay(kHudEntries);
        const auto mem = r.mod.memory();
        previous_slots = mem.read<eqs::Slots>(slots_address);
        previous_cache = mem.read<std::array<HudEntry, 8>>(cache_address);
        if (!eqs::valid_game_slots(previous_slots)) return;
        const auto definitions = r.overlay(kItemDefs);
        std::size_t validated_items = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            const auto id = static_cast<openrac::gaddr>(previous_slots[i]);
            const bool id_matches = previous_cache[i].item == previous_slots[i];
            const auto live_icon = id == 0 ? std::uint16_t{} :
                mem.read<std::uint16_t>(definitions + id * 0x4C + 0x38);
            const bool icon_matches = eqs::hud_icon_compatible(
                previous_slots[i], previous_cache[i].icon, live_icon);
            if (!id_matches || !icon_matches) {
                if (!r.warned_layout) {
                    r.mod.warn("Quick Select HUD layout mismatch; secondary page projection disabled");
                    r.warned_layout = true;
                }
                return;
            }
            if (id != 0) ++validated_items;
        }
        // All-zero memory would pass an ID-only check even at a wrong address.
        if (validated_items == 0) return;
        auto alternate_cache = previous_cache;
        for (std::size_t i = 0; i < 8; ++i) {
            const auto id = static_cast<openrac::gaddr>(second[i]);
            alternate_cache[i].item = second[i];
            alternate_cache[i].icon = mem.read<std::uint16_t>(definitions + id * 0x4C + 0x38);
        }
        mem.write(slots_address, second);
        mem.write(cache_address, alternate_cache);
        active = true;
    }
    ~Projection() {
        if (!active) return;
        state.write(slots_address, previous_slots);
        state.mod.memory().write(cache_address, previous_cache);
    }
    Projection(const Projection&) = delete;
    Projection& operator=(const Projection&) = delete;
private:
    Runtime& state;
    openrac::gaddr slots_address = 0, cache_address = 0;
    eqs::Slots previous_slots{};
    std::array<HudEntry, 8> previous_cache{};
    bool active = false;
};

// The gameplay action handler (including the Thruster-Pack R1 action) reads
// g_controller_state.held / pressed. Only mask those gameplay-facing fields
// during Hero_Update; the Quick Select hook still reads held_unmasked, so
// page switching continues to work. Restore exactly the shoulder bits after
// the hero finishes updating, without losing changes to unrelated buttons.
class ScopedHeroShoulderFilter {
public:
    explicit ScopedHeroShoulderFilter(Runtime& r) : state(r) {
        const auto pad = r.exe(kPadState);
        mask = eqs::gameplay_shoulder_mask(
            r.mod.memory().read<std::uint32_t>(pad + 0x1C0),
            r.editor.active());
        if (mask == 0) return;
        for (std::size_t i = 0; i < kOffsets.size(); ++i) {
            const auto address = pad + kOffsets[i];
            const auto current = state.mod.memory().read<std::uint32_t>(address);
            saved[i] = current & mask;
            state.mod.memory().write<std::uint32_t>(
                address, eqs::filtered_gameplay_buttons(current, mask));
        }
    }
    ~ScopedHeroShoulderFilter() {
        if (mask == 0) return;
        const auto pad = state.exe(kPadState);
        for (std::size_t i = 0; i < kOffsets.size(); ++i) {
            const auto address = pad + kOffsets[i];
            const auto current = state.mod.memory().read<std::uint32_t>(address);
            state.mod.memory().write<std::uint32_t>(
                address, (current & ~mask) | saved[i]);
        }
    }
    ScopedHeroShoulderFilter(const ScopedHeroShoulderFilter&) = delete;
    ScopedHeroShoulderFilter& operator=(const ScopedHeroShoulderFilter&) = delete;

private:
    // held, pressed, raw_held, raw_pressed from RAC1 PadState.
    static constexpr std::array<openrac::gaddr, 4> kOffsets{
        0x1A0, 0x1A4, 0x1B0, 0x1B4
    };
    Runtime& state;
    std::uint32_t mask = 0;
    std::array<std::uint32_t, kOffsets.size()> saved{};
};

// Render a small native-font hint inside the right-hand pause editor panel.
// hud_text_set() cannot do this: OpenRAC draws HUD lines behind menus.
//
// Menu_ItemText_Draw resolves a text ID from the game's help message table.
// Briefly redirect one existing ID to a private, backed-up game RAM text
// buffer, and invoke that renderer with a temporary label description on the
// *existing* Quick Select screen. All bytes are restored before returning.
// This avoids adding a permanent label to RAC1's static menu configuration.
class ScopedEditorHint {
public:
    ScopedEditorHint(Runtime& r, openrac::gaddr screen) : state(r), screen_address(screen) {
        if (!r.editor.active() || screen == 0) {
            diagnostic = "editor inactive / invalid screen";
            return;
        }
        if (!r.menu_text_draw) {
            diagnostic = "menu font callback not yet observed";
            return;
        }
        const auto mem = r.mod.memory();

        // Unlike most data in this mod, the help-message table pointer has
        // different PAL and NTSC-U addresses, NOT a uniform -0x100 offset.
        // NTSC-U's get_help_message_text explicitly reads D_0015F6A0.
        const auto pointer_address = r.mod.relocate(
            eqs::help_message_pointer_address(r.mod.version() == "ntsc"));
        const auto table = mem.read<openrac::gaddr>(pointer_address);
        const auto count = mem.read<std::int32_t>(r.exe(kHelpState) + 0x2C);
        if (table < 0x10000 || table >= 0x2000000 || count <= 0 || count > 4096 ||
            static_cast<std::uint64_t>(table) + static_cast<std::uint64_t>(count) * 16u > 0x2000000) {
            diagnostic = "message table invalid: slot=" + std::to_string(pointer_address) +
                         " ptr=" + std::to_string(table) + " count=" + std::to_string(count);
            return;
        }

        const std::string caption =
            eqs::editor_page_hint(r.editor.active_page(), r.banks.page_count());
        const std::size_t caption_size = caption.size() + 1;
        constexpr std::size_t kSavedBytes = 32;
        if (caption_size > kSavedBytes) {
            diagnostic = "footer caption exceeds temporary text space";
            return;
        }

        // Instead of relying on an unknown NTSC location for a HUD text
        // buffer, borrow the bytes of one existing, sufficiently long help
        // message only for this synchronous draw, then restore them.
        // That message's table pointer is never modified.
        openrac::gaddr found_text = 0;
        std::int32_t found_id = 0;
        std::array<std::uint8_t, kSavedBytes> original_text{};
        for (int i = 0; i < count; ++i) {
            const auto entry = static_cast<openrac::gaddr>(table + i * 16);
            const auto id = mem.read<std::int32_t>(entry + 4);
            const auto ptr = mem.read<openrac::gaddr>(entry);
            if (id <= 0 || id >= 65536 || ptr < 0x10000 ||
                static_cast<std::uint64_t>(ptr) + kSavedBytes >= 0x2000000) continue;
            const auto sample = mem.read<std::array<std::uint8_t, kSavedBytes>>(ptr);
            // The existing string must have enough space for all of the
            // caption including the terminating NUL; no overwriting the
            // neighbouring message's storage.
            bool enough = true;
            for (std::size_t j = 0; j + 1 < caption_size; ++j)
                if (sample[j] == 0) { enough = false; break; }
            if (!enough) continue;
            found_text = ptr;
            found_id = id;
            original_text = sample;
            break;
        }
        if (found_text == 0) {
            diagnostic = "no message long enough for footer (count=" +
                         std::to_string(count) + ")";
            return;
        }

        const int height = mem.read<std::int32_t>(screen + 0x24);
        const int width = mem.read<std::int32_t>(screen + 0x20);
        if (height < 120 || height > 512 || width < 120 || width > 512) {
            diagnostic = "unexpected panel size " + std::to_string(width) +
                         "x" + std::to_string(height);
            return;
        }

        saved_screen = mem.read<std::array<std::uint8_t, 0x54>>(screen);
        saved_message = original_text;
        message_address = found_text;
        active = true; // Every game-memory modification below is restored.

        auto substitute = original_text;
        std::memcpy(substitute.data(), caption.c_str(), caption_size);
        mem.write(found_text, substitute);

        // Reuse the game's small menu text renderer, instead of the HUD
        // overlay (which OpenRAC intentionally draws behind pause menus).
        constexpr std::int32_t kLabelFlags = 0x2000 | 0x10 | 0x04 | 0x01;
        mem.write<std::int32_t>(screen + 0x30, kLabelFlags);
        mem.write<std::int32_t>(screen + 0x34, found_id);
        mem.write<std::uint32_t>(screen + 0x38, 0);
        mem.write<std::int32_t>(screen + 0x3C, eqs::editor_label_scroll_16ths(height));
        mem.write<std::int32_t>(screen + 0x44, 0x100);
        mem.write<std::int32_t>(screen + 0x48, 0);
        mem.write<std::int32_t>(screen + 0x4C, 0);

        diagnostic = "native draw reached, message ID " + std::to_string(found_id) +
                     ", panel " + std::to_string(width) + "x" + std::to_string(height);
    }

    ~ScopedEditorHint() {
        if (!active) return;
        const auto mem = state.mod.memory();
        mem.write(screen_address, saved_screen);
        mem.write(message_address, saved_message);
    }
    bool ready() const noexcept { return active; }
    const std::string& outcome() const noexcept { return diagnostic; }
    ScopedEditorHint(const ScopedEditorHint&) = delete;
    ScopedEditorHint& operator=(const ScopedEditorHint&) = delete;
private:
    Runtime& state;
    openrac::gaddr screen_address;
    openrac::gaddr message_address = 0;
    std::array<std::uint8_t, 0x54> saved_screen{};
    std::array<std::uint8_t, 32> saved_message{};
    std::string diagnostic{"not attempted"};
    bool active = false;
};

std::unique_ptr<Runtime> runtime;

bool external_wheel_available() {
#ifdef _WIN32
    for (const auto name : {L"weapon_wheel_pause.dll", L"libweapon_wheel_pause.dll"}) {
        if (const auto module = GetModuleHandleW(name)) {
            using Ready = int (*)();
            const auto ready = reinterpret_cast<Ready>(
                GetProcAddress(module, "openrac_weapon_wheel_pause_eqs_bridge"));
            if (ready && ready()) return true;
        }
    }
#endif
    return false;
}

int external_page(uint32_t event, uint32_t held, eqs_wheel_page_v2* out) {
    if (!runtime) return 0;
    auto& r = *runtime;
    if (event == EQS_WHEEL_END) {
        if (r.external_wheel) r.banks.close_wheel();
        r.external_wheel = false;
        r.label(false);
        return 1;
    }
    if (!out || out->size < sizeof(*out)) return 0;
    if (event == EQS_WHEEL_BEGIN) {
        r.load_slot();
        r.refresh_primary();
        r.banks.close_wheel();
        // Seed held buttons before opening: do not count an old press.
        r.banks.switch_on_press((held & kL1) != 0, (held & kR1) != 0);
        r.banks.open_wheel();
        r.external_wheel = true;
        r.label(false);
    } else if (event == EQS_WHEEL_UPDATE && r.external_wheel && (held & kTriangle)) {
        r.banks.switch_on_press((held & kL1) != 0, (held & kR1) != 0);
    } else if (event != EQS_WHEEL_UPDATE && event != EQS_WHEEL_DRAW) {
        return 0;
    }
    if (!r.external_wheel) return 0;
    eqs_wheel_page_v2 page{};
    page.size = sizeof(page);
    page.page_index = static_cast<uint32_t>(eqs::page_index(r.banks.active_page()));
    page.page_count = static_cast<uint32_t>(r.banks.page_count());
    const auto& items = r.banks.slots(r.banks.active_page());
    for (std::size_t i = 0; i < items.size(); ++i) page.native_ids[i] = items[i];
    *out = page;
    return 1;
}


// Read-only C ABI for optional cooperating mods. Slots remain native IDs.
int compat_get_state(eqs_compat_state_v1* out) {
    if (!runtime || !out || out->size < sizeof(eqs_compat_state_v1)) return 0;
    const auto& r = *runtime;
    eqs_compat_state_v1 state{};
    state.size = sizeof(state);
    state.page_count = static_cast<std::uint32_t>(r.banks.page_count());
    state.gameplay_page = r.banks.wheel_open()
        ? static_cast<std::uint32_t>(eqs::page_index(r.banks.active_page()))
        : EQS_COMPAT_NO_PAGE;
    state.editor_page = r.editor.active()
        ? static_cast<std::uint32_t>(eqs::page_index(r.editor.active_page()))
        : EQS_COMPAT_NO_PAGE;
    state.flags = (r.banks.wheel_open() ? EQS_COMPAT_GAMEPLAY_OPEN : 0u)
                | (r.editor.active() ? EQS_COMPAT_EDITOR_OPEN : 0u);
    *out = state;
    return 1;
}
int compat_get_slots(std::uint32_t page, eqs_compat_slots_v1* out) {
    if (!runtime || !out || out->size < sizeof(eqs_compat_slots_v1) ||
        page >= runtime->banks.page_count()) return 0;
    eqs_compat_slots_v1 slots{};
    slots.size = sizeof(slots);
    slots.count = 8;
    const auto& native = runtime->banks.slots(eqs::page_at(page));
    for (std::size_t i = 0; i < native.size(); ++i) slots.native_ids[i] = native[i];
    *out = slots;
    return 1;
}
const eqs_compat_api_v1 compat_api{sizeof(eqs_compat_api_v1),
    EQS_COMPAT_API_VERSION_1, compat_get_state, compat_get_slots};

void load(openrac::Mod& mod) {
    if (mod.game() == "rac2") {
        expanded_quick_select::load_rac2(mod);
        return;
    }
    if (mod.game() != "rac1" || (mod.version() != "pal" && mod.version() != "ntsc"))
        throw openrac::Error("Expanded Quick Select requires RAC1 PAL or NTSC-U");
    if (mod.api_minor() < 13)
        throw openrac::Error("Expanded Quick Select touch controls require OpenRAC mod API 1.13+");
    runtime = std::make_unique<Runtime>(mod);
    Runtime& r = *runtime;

    // Process controller input before the hero/world tick sees it. RAC1's
    // airborne slam can be triggered by R1 OR R2, and some actions consult
    // PadState.hist_btn (recent presses), not just current held/pressed.
    // Filter all shoulder gameplay actions during Triangle Quick Select or
    // the pause editor, leaving held_unmasked/pressed_unmasked intact so
    // L1/R1 still navigate the wheel/slots and L2/R2 switch editor pages.
    mod.hook<openrac::rac1::Pad_ProcessReport>(
        [&r](auto original, std::uint32_t pad, std::uint32_t report, std::int32_t size) {
            original(pad, report, size);
            if (pad != r.exe(kPadState)) return; // only the primary controller
            const auto mem = r.mod.memory();
            const auto unmasked = mem.read<std::uint32_t>(pad + 0x1C0);
            const auto mask = eqs::gameplay_shoulder_mask(unmasked, r.editor.active());
            if (mask == 0) return;
            // Clear the most recently recorded press from the 30-frame ring.
            // On leaving Quick Select the game must not treat that R1 page
            // change as a fresh airborne slam through pad_pressed_within().
            const auto count = mem.read<std::int32_t>(pad + 0x190);
            const auto next = mem.read<std::int16_t>(pad + 0x18E);
            if (count > 0 && count <= 30 && next >= 0 && next < 30) {
                const auto last = eqs::last_history_index(
                    static_cast<std::uint32_t>(next));
                const auto addr = static_cast<openrac::gaddr>(pad + 0x1E0 + last * 4);
                const auto pressed = mem.read<std::uint32_t>(addr);
                mem.write<std::uint32_t>(
                    addr, eqs::filtered_gameplay_buttons(pressed, mask));
            }
            // Also prevent other per-frame world/gameplay handlers from
            // seeing the same wheel-only shoulder presses.
            constexpr std::array<openrac::gaddr, 4> offsets{
                0x1A0, 0x1A4, 0x1B0, 0x1B4
            };
            for (const auto offset : offsets) {
                const auto addr = static_cast<openrac::gaddr>(pad + offset);
                const auto buttons = mem.read<std::uint32_t>(addr);
                mem.write<std::uint32_t>(
                    addr, eqs::filtered_gameplay_buttons(buttons, mask));
            }
        });

    mod.hook<openrac::rac1::Hero_Update>([&r](auto original) {
        ScopedHeroShoulderFilter filter(r);
        original();
    });

    mod.hook<openrac::rac1::Hud_QuickSelect_Init>([&r](auto original, std::uint32_t selector) {
        original(selector);
        if (external_wheel_available()) return;
        r.load_slot();
        r.refresh_primary();
    });
    mod.hook<openrac::rac1::Hud_QuickSelect_Update>([&r](auto original, std::uint32_t selector) {
        if (external_wheel_available()) { original(selector); return; }
        r.load_slot();
        const auto pad = r.exe(kPadState);
        const auto held = r.mod.memory().read<std::uint32_t>(pad + 0x1C0);
        const bool triangle = (held & kTriangle) != 0;
        r.wheel.before_update(r.banks, triangle, (held & kL1) != 0, (held & kR1) != 0);
        {
            Projection projection(r);
            original(selector);
        }
        r.wheel.after_update(r.banks, triangle);
        r.label(triangle);
    });
    mod.hook<openrac::rac1::Hud_QuickSelect_Draw>([&r](auto original, std::uint32_t selector) {
        if (external_wheel_available()) return original(selector);
        Projection projection(r);
        return original(selector);
    });

    mod.hook<openrac::rac1::Menu_ItemText_Draw>(
        [&r](auto original, std::uint32_t label) {
            // Keep the original game's function pointer for the ring's
            // extra hint, which needs the game's font and menu layer.
            if (!r.menu_text_draw && r.editor.active())
                r.footer_note("game text renderer was called");
            r.menu_text_draw = original.get();
            return original(label);
        });
    mod.hook<openrac::rac1::Menu_QuickSelect_Draw>(
        [&r](auto original, std::uint32_t screen) {
            const auto result = original(screen);
            if (r.editor.active()) {
                // A footer failure must not cause the SDK to repeat the
                // original ring draw, or leave borrowed menu memory changed.
                try {
                    ScopedEditorHint footer(r, screen);
                    if (footer.ready()) r.menu_text_draw(screen);
                    r.footer_note(footer.outcome());
                } catch (const std::exception& e) {
                    r.mod.warn(std::string("Quick Select footer skipped: ") + e.what());
                }
            }
            return result;
        });

    mod.hook<openrac::rac1::Menu_QuickSelect_Load>([&r](auto original, std::uint32_t page) {
        const auto result = original(page);
        const auto visible = r.read(page + 0x30);
        if (eqs::valid_game_slots(visible)) r.banks.set_primary(visible);
        r.editor.begin(r.banks);
        r.editor_address = page;
        // OpenRAC 1.13+ shows custom touch L2/R2 buttons only while
        // editing a Quick Select loadout with more than one wheel.
        // These send real shoulder presses, not simulated HUD clicks.
        r.mod.touch_hint("eqs_pages", r.banks.page_count() > 1);
        r.footer_diagnostic.clear();
        r.mod.info("Quick Select footer: editor opened");
        return result;
    });
    mod.hook<openrac::rac1::Menu_ItemSlots_Update>([&r](auto original, std::uint32_t page) {
        if (r.editor.active() && r.editor_address == page) {
            const auto pressed = r.mod.memory().read<std::uint32_t>(r.exe(kPadState) + 0x1C4);
            const auto visible = r.read(page + 0x30);
            if (r.editor.switch_page(r.banks, (pressed & kL2) != 0,
                                     (pressed & kR2) != 0, visible)) {
                const auto& next = r.editor.visible_slots(r.banks);
                r.write(page + 0x30, next);
                std::int32_t cursor = 0;
                while (cursor < 8 && next[cursor] != 0) ++cursor;
                r.mod.memory().write<std::int32_t>(page + 0x50, cursor % 8);
            }
        }
        const auto result = original(page);
        if (r.editor.active() && r.editor_address == page)
            r.editor.capture(r.banks, r.read(page + 0x30));
        return result;
    });
    mod.hook<openrac::rac1::Menu_QuickSelect_Store>([&r](auto original, std::uint32_t page) {
        if (!r.editor.active() || r.editor_address != page) return original(page);
        r.editor.capture(r.banks, r.read(page + 0x30));
        r.write(r.exe(kSavedSlots), r.banks.slots(eqs::Page::Primary));
        r.save_extra_pages();
        r.editor.end();
        r.mod.touch_hint("eqs_pages", false);
        r.editor_address = 0;
        return std::int32_t{0};
    });
    mod.hook<openrac::rac1::Save_Prepare>([&r](auto original, std::int32_t a, std::int32_t b) {
        original(a, b);
        r.save_extra_pages();
    });
    mod.hook<openrac::rac1::Save_Restore>([&r](auto original, std::uint32_t arg) {
        original(arg);
        r.reset(false);
    });
    mod.hook<openrac::rac1::Save_NewGame>([&r](auto original) {
        original();
        r.reset(true);
    });
    mod.info("Expanded Quick Select: " + std::to_string(r.banks.page_count()) +
             " wheel(s); L1/R1 gameplay, L2/R2 pause editor");
}
} // namespace

// Discover from the already loaded module; never instantiate a second mod.
#if defined(_WIN32)
extern "C" __declspec(dllexport)
#else
extern "C" __attribute__((visibility("default")))
#endif
const eqs_compat_api_v1* openrac_expanded_quick_select_query(std::uint32_t version) {
    return version == EQS_COMPAT_API_VERSION_1 && runtime ? &compat_api : nullptr;
}

#if defined(_WIN32)
extern "C" __declspec(dllexport)
#else
extern "C" __attribute__((visibility("default")))
#endif
int openrac_eqs_wheel_page_v2(uint32_t event, uint32_t held, eqs_wheel_page_v2* out) {
    try { return external_page(event, held, out); }
    catch (...) { return 0; }
}

OPENRAC_MOD_CPP(expanded_quick_select::kModId,
                expanded_quick_select::kModVersion, load)
