#include "expanded_quick_select/identity.hpp"
#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/slot_store.hpp"
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
        : mod(m), store(m.data_dir()), shift(m.version() == "ntsc" ? -0x100 : 0) {}
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
        banks.set_secondary(store.load(slot).value_or(eqs::Slots{}));
        refresh_primary();
    }
    void save_page_two() {
        const int slot = mod.save_slot_current();
        if (slot >= 0 && !store.save(slot, banks.slots(eqs::Page::Secondary)))
            mod.warn("Could not persist second page for save " + std::to_string(slot));
        last_save_slot = slot;
    }
    void label(bool visible) {
        if (!visible && !label_visible) return;
        label_visible = visible;
        if (!visible) { mod.hud_text_set({}); return; }
        openrac_hud_text text{};
        const char* message = banks.active_page() == eqs::Page::Primary ? "PAGE 1 / 2" : "PAGE 2 / 2";
        std::memcpy(text.text, message, std::strlen(message) + 1);
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
        if (r.banks.active_page() != eqs::Page::Secondary) return;
        const auto second = r.banks.slots(eqs::Page::Secondary);
        if (!eqs::valid_game_slots(second)) return;
        slots_address = r.exe(kSavedSlots);
        cache_address = r.overlay(kHudEntries);
        const auto mem = r.mod.memory();
        previous_slots = mem.read<eqs::Slots>(slots_address);
        previous_cache = mem.read<std::array<HudEntry, 8>>(cache_address);
        if (!eqs::valid_game_slots(previous_slots)) return;
        for (std::size_t i = 0; i < 8; ++i) {
            if (previous_cache[i].item != previous_slots[i]) {
                if (!r.warned_layout) {
                    r.mod.warn("Quick Select HUD layout mismatch; page 2 projection disabled");
                    r.warned_layout = true;
                }
                return;
            }
        }
        auto alternate_cache = previous_cache;
        const auto definitions = r.overlay(kItemDefs);
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

std::unique_ptr<Runtime> runtime;

void load(openrac::Mod& mod) {
    if (mod.game() != "rac1" || (mod.version() != "pal" && mod.version() != "ntsc"))
        throw openrac::Error("Expanded Quick Select requires RAC1 PAL or NTSC-U");
    if (mod.api_minor() < 12)
        throw openrac::Error("Expanded Quick Select requires OpenRAC mod API 1.12+");
    runtime = std::make_unique<Runtime>(mod);
    Runtime& r = *runtime;

    mod.hook<openrac::rac1::Hud_QuickSelect_Init>([&r](auto original, std::uint32_t selector) {
        original(selector);
        r.load_slot();
        r.refresh_primary();
    });
    mod.hook<openrac::rac1::Hud_QuickSelect_Update>([&r](auto original, std::uint32_t selector) {
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
        Projection projection(r);
        return original(selector);
    });

    mod.hook<openrac::rac1::Menu_QuickSelect_Load>([&r](auto original, std::uint32_t page) {
        const auto result = original(page);
        const auto visible = r.read(page + 0x30);
        if (eqs::valid_game_slots(visible)) r.banks.set_primary(visible);
        r.editor.begin(r.banks);
        r.editor_address = page;
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
        r.save_page_two();
        r.editor.end();
        r.editor_address = 0;
        return std::int32_t{0};
    });
    mod.hook<openrac::rac1::Save_Prepare>([&r](auto original, std::int32_t a, std::int32_t b) {
        original(a, b);
        r.save_page_two();
    });
    mod.hook<openrac::rac1::Save_Restore>([&r](auto original, std::uint32_t arg) {
        original(arg);
        r.reset(false);
    });
    mod.hook<openrac::rac1::Save_NewGame>([&r](auto original) {
        original();
        r.reset(true);
    });
    mod.info("Expanded Quick Select dev hooks enabled: L1/R1 wheel; L2/R2 pause editor");
}
} // namespace

OPENRAC_MOD_CPP(expanded_quick_select::kModId,
                expanded_quick_select::kModVersion, load)
