#include "expanded_quick_select/rac2.hpp"
#include "expanded_quick_select/banks.hpp"
#include "expanded_quick_select/gameplay_hint.hpp"
#include "expanded_quick_select/rac2_input.hpp"
#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/slot_store.hpp"
#include "openrac/games/rac2.hpp"
#include "openrac/mod.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace expanded_quick_select {
namespace {

// Addresses verified for NTSC-U v1.01 in OpenRAC/rac2-decomp and
// OpenRAC's port/game/rac2-ntsc/host/rac2_addresses.h.
constexpr openrac::gaddr kNativeSlots = 0x001A73B8;
constexpr openrac::gaddr kHudCache = 0x00255170;
constexpr openrac::gaddr kItemMap = 0x00139568;
constexpr openrac::gaddr kItemRecords = 0x00239BA0;
constexpr openrac::gaddr kInventoryOrder = 0x001A7B68;
constexpr std::int32_t kRac2MaxItemId = 55;
constexpr std::uint32_t kRecordStride = 0xE0;

struct CacheEntry {
    std::uint32_t icon;
    std::uint8_t padding[20];
    std::int32_t item;
};
static_assert(sizeof(CacheEntry) == 0x1C, "RAC2 cache layout changed");
using Cache = std::array<CacheEntry, 8>;

class Runtime {
public:
    explicit Runtime(openrac::Mod& m)
        : mod(m), store(m.data_dir() / "rac2", kRac2MaxItemId),
          banks({}, kRac2MaxItemId) {
        const auto count = m.setting<std::string>("wheel_count");
        banks.set_page_count(count.size() == 1 && count[0] >= '1' && count[0] <= '8'
                             ? static_cast<std::size_t>(count[0] - '0') : 2);
    }

    void on_pad(int port, std::uint8_t* report, int size) {
        if (port != 0 || !report || size < 2) return;
        // Never leave a projected wheel in the game's canonical save slots.
        // The regular restore is in on_frame, after the game's tick.
        restore();
        const auto held = rac2_input::held(report, size);
        const bool triangle = (held & rac2_input::kTriangle) != 0;
        const bool was_open = banks.wheel_open();
        const bool l1 = (held & rac2_input::kL1) != 0;
        const bool r1 = (held & rac2_input::kR1) != 0;
        last_triangle = triangle;
        wheel.before_update(banks, triangle, l1, r1);

        if (mod.loaded_level() < 0) return;
        refresh_slot();
        const auto current = mod.memory().read<Slots>(mod.relocate(kNativeSlots));
        if (!valid_game_slots(current, kRac2MaxItemId)) return;
        banks.set_primary(current);

        // On the release tick the game still needs the selected page to
        // resolve the highlighted item; the last page closes in on_frame.
        if ((triangle || was_open) && banks.active_page() != Page::Primary) {
            project(current, banks.slots(banks.active_page()));
        }

        if (triangle && banks.page_count() > 1) {
            rac2_input::consume_page_buttons(report, size);
        }
    }

    void on_frame() {
        restore();
        wheel.after_update(banks, last_triangle);
        if (mod.loaded_level() >= 0) refresh_slot();
        if (!banks.wheel_open() || banks.page_count() < 2) {
            mod.hud_text_set({});
        } else {
            openrac_hud_text line{};
            const auto message = gameplay_page_hint(banks.active_page(), banks.page_count());
            if (message.size() + 1 <= sizeof(line.text)) {
                std::memcpy(line.text, message.c_str(), message.size() + 1);
                line.x = 0.5f;
                line.y = 0.81f;
                line.scale = 0.62f;
                line.rgba[0] = line.rgba[1] = line.rgba[2] = 220;
                line.rgba[3] = 255;
                line.flags = OPENRAC_HUD_SMALL | OPENRAC_HUD_SHADOW;
                mod.hud_text_set({line});
            }
        }
    }

    void on_item_give(std::int32_t item) {
        if (item <= 0 || item > kRac2MaxItemId ||
            mod.loaded_level() < 0 || banks.page_count() < 2) return;
        restore();
        refresh_slot();
        const auto mem = mod.memory();
        const auto primary = mem.read<Slots>(mod.relocate(kNativeSlots));
        if (!valid_game_slots(primary, kRac2MaxItemId)) return;
        banks.set_primary(primary);

        // Only queue entries that appear in the game's weapon/gadget list.
        // Flagged order bytes retain the native item in their low 7 bits.
        const auto inventory = mem.read<std::array<std::uint8_t, 32>>(
            mod.relocate(kInventoryOrder));
        const bool eligible = std::any_of(inventory.begin(), inventory.end(),
            [item](std::uint8_t v) {
                return v != 0xFF && (v & 0x7F) == static_cast<std::uint32_t>(item);
            });
        if (!eligible) return;
        for (std::size_t p = 0; p < banks.page_count(); ++p) {
            const auto& page = banks.slots(page_at(p));
            if (std::find(page.begin(), page.end(), item) != page.end()) return;
        }
        for (std::size_t p = 1; p < banks.page_count(); ++p) {
            const auto& page = banks.slots(page_at(p));
            const auto empty = std::find(page.begin(), page.end(), 0);
            if (empty == page.end()) continue;
            const auto at = static_cast<std::size_t>(empty - page.begin());
            if (!banks.set_slot(page_at(p), at, item)) return;
            if (mod.save_slot_current() >= 0 &&
                !store.save(mod.save_slot_current(), page_at(p), banks.slots(page_at(p)))) {
                mod.warn("RAC2: could not save new extra-wheel item");
            }
            mod.info("RAC2: added item " + std::to_string(item) + " to page " +
                     std::to_string(p + 1));
            return;
        }
    }

private:
    void refresh_slot() {
        const int slot = mod.save_slot_current();
        if (slot == last_save_slot) return;
        last_save_slot = slot;
        for (std::size_t p = 1; p < kMaxPages; ++p) {
            banks.set_page(page_at(p), store.load(slot, page_at(p)).value_or(Slots{}));
        }
        banks.close_wheel();
        mod.info("RAC2: loaded extra wheels for save slot " + std::to_string(slot));
    }

    openrac::gaddr level_data(openrac::gaddr address) const {
        return mod.relocate_level(mod.loaded_level(), address);
    }

    // Builds the original game's icon value from the same table and record
    // offsets used by decompiled func_0028BCE8.
    std::uint32_t icon_for(std::int32_t item) const {
        const auto mem = mod.memory();
        const auto mapping = mem.read<std::uint8_t>(
            mod.relocate(kItemMap) + static_cast<openrac::gaddr>(item));
        return mem.read<std::uint16_t>(
            level_data(kItemRecords) + mapping * kRecordStride + 0x3C);
    }

    void project(const Slots& primary, const Slots& secondary) {
        if (!valid_game_slots(secondary, kRac2MaxItemId)) return;
        const auto mem = mod.memory();
        const auto cache_addr = level_data(kHudCache);
        const auto cache = mem.read<Cache>(cache_addr);
        // If the cache does not match the canonical page, do not poke
        // unknown/relocated menu data. Requires a live game verification.
        bool good = false;
        for (std::size_t i = 0; i < primary.size(); ++i) {
            if (cache[i].item != primary[i] ||
                cache[i].icon != icon_for(primary[i])) {
                if (!warned_cache) {
                    mod.warn("RAC2 HUD cache mismatch; skipping secondary page projection");
                    warned_cache = true;
                }
                return;
            }
            good = good || primary[i] != 0;
        }
        if (!good) return;
        auto replacement = cache;
        for (std::size_t i = 0; i < secondary.size(); ++i) {
            replacement[i].item = secondary[i];
            replacement[i].icon = icon_for(secondary[i]);
        }
        previous_slots = primary;
        previous_cache = cache;
        mod.memory().write(mod.relocate(kNativeSlots), secondary);
        mod.memory().write(cache_addr, replacement);
        projected_cache = cache_addr;
        projection_active = true;
    }

    void restore() {
        if (!projection_active) return;
        mod.memory().write(mod.relocate(kNativeSlots), previous_slots);
        mod.memory().write(projected_cache, previous_cache);
        projection_active = false;
    }

    openrac::Mod& mod;
    SlotStore store;
    Banks banks;
    WheelSession wheel;
    int last_save_slot = -2;
    bool last_triangle = false;
    bool warned_cache = false;
    bool projection_active = false;
    openrac::gaddr projected_cache = 0;
    Slots previous_slots{};
    Cache previous_cache{};
};

std::unique_ptr<Runtime> runtime;
} // namespace

void load_rac2(openrac::Mod& mod) {
    if (mod.version() != "ntsc")
        throw openrac::Error("RAC2 experimental support is currently NTSC-U v1.01 only");
    if (mod.api_minor() < 20)
        throw openrac::Error("RAC2 dev build requires OpenRAC mod API 1.20+");
    if (!mod.setting<bool>("rac2_experimental")) {
        mod.info("Expanded Quick Select RAC2 prototype is disabled; enable experimental RAC2 pages to test");
        return;
    }
    runtime = std::make_unique<Runtime>(mod);
    Runtime& r = *runtime;
    mod.on_pad([&r](int port, std::uint8_t* report, int size) {
        r.on_pad(port, report, size);
    });
    mod.on_frame([&r]() { r.on_frame(); });
    mod.hook<openrac::rac2::Item_Give>(
        [&r](auto original, std::int32_t item) {
            original(item);
            r.on_item_give(item);
        });
    mod.warn("RAC2 NTSC-U Quick Select is experimental: in-game validation needed, pause editor not supported");
}
} // namespace expanded_quick_select
