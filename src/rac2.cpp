#include "expanded_quick_select/rac2.hpp"
#include "expanded_quick_select/banks.hpp"
#include "expanded_quick_select/gameplay_hint.hpp"
#include "expanded_quick_select/rac2_input.hpp"
#include "expanded_quick_select/rac2_inventory.hpp"
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
constexpr openrac::gaddr kInventoryOwned = 0x001A7AF8;
constexpr std::int32_t kRac2MaxItemId = rac2_inventory::kMaxItemId;
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
        : mod(m), store(std::filesystem::path(m.data_dir()) / "rac2", kRac2MaxItemId),
          banks({}, kRac2MaxItemId) {
        const auto count = m.setting<std::string>("wheel_count");
        banks.set_page_count(count.size() == 1 && count[0] >= '1' && count[0] <= '8'
                             ? static_cast<std::size_t>(count[0] - '0') : 2);
    }

    void on_pad(int port, std::uint8_t* report, int size) {
        if (port != 0 || !report || size < 2) return;
        restore();
        const auto held = rac2_input::held(report, size);
        const bool triangle = (held & rac2_input::kTriangle) != 0;
        const bool l1 = (held & rac2_input::kL1) != 0;
        const bool r1 = (held & rac2_input::kR1) != 0;
        last_l1 = l1;
        last_r1 = r1;
        const auto status = mod.play_status();
        if (status.phase != OPENRAC_PLAY_PLAYING || mod.loaded_level() < 0) {
            cancel_wheel(l1, r1);
            return;
        }
        // Refresh before opening so loading a sidecar cannot cancel the session.
        refresh_slot();
        const auto current = mod.memory().read<Slots>(mod.relocate(kNativeSlots));
        if (!valid_game_slots(current, kRac2MaxItemId)) {
            cancel_wheel(l1, r1);
            return;
        }
        banks.set_primary(current);
        const bool was_open = banks.wheel_open();
        last_triangle = triangle;
        // Don't change page on the release tick: commit the last shown page.
        if (triangle || !was_open) wheel.before_update(banks, triangle, l1, r1);
        if ((triangle || was_open) && banks.wheel_open() &&
            banks.active_page() != Page::Primary) {
            auto page = banks.slots(banks.active_page());
            const auto owned = mod.memory().read<rac2_inventory::Owned>(
                mod.relocate(kInventoryOwned));
            for (auto& item : page) {
                if (!rac2_inventory::is_owned(owned, item)) item = 0;
            }
            if (!project(current, page, status.loads)) {
                banks.close_wheel();
                if (triangle) banks.open_wheel();
                return; // preserve native input on a cache mismatch
            }
        }
        if (triangle && banks.page_count() > 1 && banks.wheel_open()) {
            rac2_input::consume_page_buttons(report, size);
        }
    }

    void on_frame() {
        restore();
        const auto status = mod.play_status();
        if (status.phase != OPENRAC_PLAY_PLAYING || mod.loaded_level() < 0) {
            cancel_wheel(false, false);
            mod.hud_text_set({});
            return;
        }
        refresh_slot();
        wheel.after_update(banks, last_triangle);
        if (!last_triangle) {
            wheel.before_update(banks, false, last_l1, last_r1);
            sync_inventory();
        }
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

    // Native grants must see the canonical slots; inventory sync happens later.
    void before_item_give() { restore(); }

private:
    void cancel_wheel(bool l1, bool r1) {
        banks.close_wheel();
        wheel.before_update(banks, false, l1, r1);
        last_triangle = false;
    }

    void sync_inventory() {
        if (banks.page_count() < 2) return;
        const auto mem = mod.memory();
        const auto primary = mem.read<Slots>(mod.relocate(kNativeSlots));
        if (!valid_game_slots(primary, kRac2MaxItemId)) return;
        banks.set_primary(primary);
        const auto order = mem.read<rac2_inventory::Order>(mod.relocate(kInventoryOrder));
        const auto owned = mem.read<rac2_inventory::Owned>(mod.relocate(kInventoryOwned));
        const auto changed = rac2_inventory::fill_owned_pages(banks, order, owned);
        for (std::size_t p = 1; p < banks.page_count(); ++p) {
            if (changed[p]) dirty_pages[p] = true;
            if (!dirty_pages[p] || last_save_slot < 0) continue;
            if (store.save(last_save_slot, page_at(p), banks.slots(page_at(p)))) {
                dirty_pages[p] = false;
                mod.info("RAC2: saved populated extra wheel " + std::to_string(p + 1));
            } else if (!warned_store) {
                warned_store = true;
                mod.warn("RAC2: could not save extra wheel; will retry");
            }
        }
    }

    void refresh_slot() {
        const int slot = mod.save_slot_current();
        if (slot == last_save_slot) return;
        last_save_slot = slot;
        dirty_pages.fill(false);
        warned_store = false;
        warned_cache = false;
        wheel = WheelSession{};
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

    bool project(const Slots& primary, const Slots& secondary, std::uint32_t loads) {
        if (!valid_game_slots(secondary, kRac2MaxItemId)) return false;
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
                    mod.warn("RAC2 HUD cache mismatch at slot " + std::to_string(i) +
                             ": native item " + std::to_string(primary[i]) +
                             ", cache item " + std::to_string(cache[i].item) +
                             ", cache icon " + std::to_string(cache[i].icon) +
                             ", expected icon " + std::to_string(icon_for(primary[i])) +
                             "; skipping extra page");
                    warned_cache = true;
                }
                return false;
            }
            good = good || primary[i] != 0;
        }
        if (!good) return false;
        auto replacement = cache;
        for (std::size_t i = 0; i < secondary.size(); ++i) {
            replacement[i].item = secondary[i];
            replacement[i].icon = icon_for(secondary[i]);
        }
        previous_slots = primary;
        previous_cache = cache;
        published_slots = secondary;
        published_cache = replacement;
        projected_slots = mod.relocate(kNativeSlots);
        projected_cache = cache_addr;
        projected_loads = loads;
        mem.write(projected_slots, secondary);
        mem.write(cache_addr, replacement);
        projection_active = true;
        if (warned_cache) {
            mod.info("RAC2: native HUD cache matched; extra pages available");
            warned_cache = false;
        }
        return true;
    }

    void restore() {
        if (!projection_active) return;
        projection_active = false;
        const auto mem = mod.memory();
        // Undo fields still carrying the projection, not native changes.
        auto slots = mem.read<Slots>(projected_slots);
        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (slots[i] == published_slots[i]) slots[i] = previous_slots[i];
        }
        mem.write(projected_slots, slots);
        // Overlay memory can be replaced by a level load.
        if (mod.loaded_level() < 0 || mod.play_status().loads != projected_loads ||
            level_data(kHudCache) != projected_cache) return;
        auto cache = mem.read<Cache>(projected_cache);
        for (std::size_t i = 0; i < cache.size(); ++i) {
            if (cache[i].item == published_cache[i].item &&
                cache[i].icon == published_cache[i].icon) {
                cache[i].item = previous_cache[i].item;
                cache[i].icon = previous_cache[i].icon;
            }
        }
        mem.write(projected_cache, cache);
    }

    openrac::Mod& mod;
    SlotStore store;
    Banks banks;
    WheelSession wheel;
    int last_save_slot = -2;
    bool last_triangle = false;
    bool last_l1 = false, last_r1 = false;
    bool warned_cache = false;
    bool projection_active = false;
    bool warned_store = false;
    std::array<bool, kMaxPages> dirty_pages{};
    openrac::gaddr projected_slots = 0;
    openrac::gaddr projected_cache = 0;
    std::uint32_t projected_loads = 0;
    Slots previous_slots{}, published_slots{};
    Cache previous_cache{}, published_cache{};
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
            r.before_item_give();
            original(item);
        });
    mod.warn("RAC2 NTSC-U Quick Select is experimental: in-game validation needed, pause editor not supported");
}
} // namespace expanded_quick_select
