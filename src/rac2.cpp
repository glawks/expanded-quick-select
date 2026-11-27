#include "expanded_quick_select/rac2.hpp"
#include "expanded_quick_select/banks.hpp"
#include "expanded_quick_select/gameplay_hint.hpp"
#include "expanded_quick_select/rac2_input.hpp"
#include "expanded_quick_select/rac2_close.hpp"
#include "expanded_quick_select/rac2_inventory.hpp"
#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/slot_store.hpp"
#include "openrac/games/rac2.hpp"
#include "openrac/mod.hpp"
#include <algorithm>
#include <array>
#include <atomic>
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
constexpr openrac::gaddr kRadialPages = 0x001B18D4;
constexpr openrac::gaddr kRadialPageIndex = 0x001A8D48;
constexpr openrac::gaddr kRadialSlotCount = 0x001B18D0;
// Despite its name, weapon_wheel_update_select writes this on native close,
// after the selected weapon has been committed.
constexpr openrac::gaddr kWheelCloseFrame = 0x001B18F8;
constexpr openrac::gaddr kWheelClosedSelection = 0x001B1900;
// g_hero_state + 0x22C8 (g_slot_index_first): native pending item.
// Read only, for diagnostics of the actual commit.
constexpr openrac::gaddr kPendingItem = 0x0018C0E8;
constexpr openrac::gaddr kEquippedItem = 0x0018B068;
constexpr unsigned kMaxClosingFrames = 120;
constexpr openrac::gaddr kGuestRamSize = 0x02000000;
constexpr openrac::gaddr kItemMap = 0x00139568;
constexpr openrac::gaddr kItemRecords = 0x00239BA0;
constexpr openrac::gaddr kInventoryOrder = 0x001A7B68;
constexpr openrac::gaddr kInventoryOwned = 0x001A7AF8;
// Same discriminator and relocation as OpenRAC's RAC2 host/touch.c:
// state 4 is a dialog; pending arg 7 is specifically Quick Select.
constexpr openrac::gaddr kGameState = 0x001A8BB0;
constexpr openrac::gaddr kStateArg = 0x001AA0F8;
constexpr openrac::gaddr kPauseState = 0x001F2840;
constexpr openrac::gaddr kResidentBase = 0x001A8CFC;
constexpr openrac::gaddr kEditorObjectOffset = 0x3A388;
constexpr openrac::gaddr kEditorCursorOffset = 0x7CC;
constexpr openrac::gaddr kEditorCallback = 0x002CD660;
constexpr std::int32_t kPauseGameState = 3;
constexpr std::int32_t kDialogState = 4;
constexpr std::int32_t kQuickSelectArg = 7;
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
          custom_store(std::filesystem::path(m.data_dir()) / "rac2" / "custom", kRac2MaxItemId),
          banks({}, kRac2MaxItemId) {
        const auto count = m.setting<std::string>("wheel_count");
        const auto profile = m.setting<std::string>("rac2_layout_profile");
        if (profile.size() == 1 && profile[0] >= '1' && profile[0] <= '4')
            manual_profile = 100 + (profile[0] - '1');
        banks.set_page_count(count.size() == 1 && count[0] >= '1' && count[0] <= '8'
                             ? static_cast<std::size_t>(count[0] - '0') : 2);
    }

    void on_pad(int port, std::uint8_t* report, int size) {
        if (port != 0 || !report || size < 2) return;
        const auto held = rac2_input::held(report, size);
        const bool triangle = (held & rac2_input::kTriangle) != 0;
        const bool l1 = (held & rac2_input::kL1) != 0;
        const bool r1 = (held & rac2_input::kR1) != 0;
        last_l1 = l1;
        last_r1 = r1;
        const auto status = mod.play_status();
        if (handle_close(status, triangle, false)) {
            if (closing) rac2_input::consume_page_buttons(report, size);
            return;
        }
        restore();
        if (handle_editor(status, held, report, size, true)) return;
        if (!wheel_phase(status)) {
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
        if (native_wheel_menu(status) && triangle && !banks.wheel_open()) {
            banks.open_wheel();
            wheel = WheelSession{};
        }
        const bool was_open = banks.wheel_open();
        last_triangle = triangle;
        // Don't change page on the release tick: commit the last shown page.
        const auto previous_page = banks.active_page();
        if (triangle || !was_open) wheel.before_update(banks, triangle, l1, r1);
        if (previous_page != banks.active_page())
            mod.info("RAC2: requested page " +
                     std::to_string(page_index(banks.active_page()) + 1));
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
        const auto status = mod.play_status();
        if (handle_close(status, last_triangle, true)) return;
        restore();
        if (handle_editor(status, 0, nullptr, 0, false)) return;
        if (!wheel_phase(status)) {
            cancel_wheel(last_l1, last_r1);
            return;
        }
        refresh_slot();
        if (native_wheel_menu(status) && last_triangle && !banks.wheel_open()) {
            banks.open_wheel();
        }
        // Native paused wheel commits the selection after its release/fade.
        // Keep the selected page until the native mode exits, not just until
        // the first pad report with Triangle released.
        if (!last_triangle && !native_wheel_menu(status)) {
            native_seen = false;
            wheel.after_update(banks, false);
            wheel.before_update(banks, false, last_l1, last_r1);
            if (status.phase == OPENRAC_PLAY_PLAYING) sync_inventory();
        }
        visible_page.store(banks.wheel_open() && banks.page_count() > 1
            ? static_cast<int>(page_index(banks.active_page()) + 1) : 0,
            std::memory_order_relaxed);
    }

    // Unlike hud_text_set, this overlay is drawn over the native menu phase.
    // Only the atomic page number crosses to the render callback.
    void draw_overlay(const openrac_overlay* overlay) const {
        const int page = visible_page.load(std::memory_order_relaxed);
        if (!overlay || page == 0 || !overlay->text || !overlay->text_width) return;
        const bool editing = page < 0;
        const auto message = editing
            ? "L2  PAGE " + std::to_string(-page) + " / " +
                std::to_string(banks.page_count()) + "  R2"
            : gameplay_page_hint(page_at(static_cast<std::size_t>(page - 1)), banks.page_count());
        constexpr float height = 12.0f;
        const float width = overlay->text_width(overlay->context, height, message.c_str());
        const float x = (512.0f - width) * 0.5f;
        const float y = editing ? 414.0f : 363.0f;
        overlay->text(overlay->context, x + 1, y + 1, height, 0x000000CCu, message.c_str());
        overlay->text(overlay->context, x, y, height, 0xDCDCDCFFu, message.c_str());
        if (editing) {
            const char* hint = "SQUARE / X: CLEAR SLOT";
            const auto w = overlay->text_width(overlay->context, 10.0f, hint);
            overlay->text(overlay->context, (512.0f - w) * 0.5f, y + 14,
                          10.0f, 0xDCDCDCFFu, hint);
        }
    }

    // Native grants must see the canonical slots; inventory sync happens later.
    void before_item_give() { restore(); }

private:
    // Identify the exact pause editor by its registered input callback,
    // not by generic pause phase, menu text, or the radial cache alone.
    // The page's 14 widgets are walked by menu_update_pause_widgets.
    bool editor_context(const openrac_play_status& status) {
        if (mod.loaded_level() < 0 || status.phase != OPENRAC_PLAY_MENU) return false;
        const auto mem = mod.memory();
        if (mem.read<std::int32_t>(mod.relocate(kGameState)) != kPauseGameState) return false;
        const auto pause = mod.relocate(kPauseState);
        if (!guest_range(pause, 0x1C)) return false;
        const auto source = mem.read<openrac::gaddr>(pause + 0x14);
        if (!guest_range(source, 0x124) || source % 4 != 0) return false;
        using Fn = int (*)();
        const auto expected = mod.function<Fn>("func_002CD660");
        const auto named = mod.function<Fn>("menu_handle_input_with_fallback");
        bool matched = false;
        std::string callbacks;
        for (unsigned i = 0; i < 14; ++i) {
            const auto widget = mem.read<openrac::gaddr>(source + 0xEC + i * 4);
            if (!guest_range(widget, 4) || widget % 4 != 0) continue;
            const auto callback = mem.read<openrac::gaddr>(widget);
            if (!guest_range(callback, 4) || callback % 4 != 0) continue;
            const auto fn = mod.function_at<Fn>(callback);
            if (callback == kEditorCallback || (fn && (fn == expected || fn == named)))
                matched = true;
            if (source != last_editor_source) callbacks += " " + std::to_string(callback);
        }
        if (source != last_editor_source) {
            last_editor_source = source;
            mod.info("RAC2: pause source " + std::to_string(source) +
                     " callbacks:" + callbacks + (matched ? " (Quick Select editor)" : ""));
        }
        // Native page is initialised once its widget count becomes nonzero.
        return matched && mem.read<std::int32_t>(pause + 4) > 0;
    }

    bool read_editor_slots(Slots& result) const {
        if (!editor_cache || live_cache() != editor_cache) return false;
        const auto cache = mod.memory().read<Cache>(editor_cache);
        for (std::size_t i = 0; i < result.size(); ++i) {
            result[i] = cache[i].item;
            if (result[i] < 0 || result[i] > kRac2MaxItemId ||
                cache[i].icon != icon_for(result[i])) return false;
        }
        return true;
    }

    bool write_editor_slots(const Slots& slots) {
        if (!editor_cache || live_cache() != editor_cache ||
            !valid_game_slots(slots, kRac2MaxItemId)) return false;
        const auto mem = mod.memory();
        auto cache = mem.read<Cache>(editor_cache);
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto icon = icon_for(slots[i]);
            if (slots[i] != 0 && (icon == 0 || icon == 0xEA7E)) return false;
            cache[i].item = slots[i];
            cache[i].icon = icon;
        }
        mem.write(editor_cache, cache);
        return true;
    }

    void capture_editor() {
        Slots current{};
        if (!read_editor_slots(current)) return;
        const auto p = page_at(editor_page);
        if (current == banks.slots(p)) return;
        banks.set_page(p, current);
        if (editor_page == 0) {
            // Native sync happens on exit. Also keep canonical slots current
            // while editing, so an extra page can never replace them.
            mod.memory().write(mod.relocate(kNativeSlots), current);
        } else {
            customised[editor_page] = true;
            dirty_pages[editor_page] = true;
            if (last_save_slot >= 0 &&
                custom_store.save(last_save_slot, p, current))
                dirty_pages[editor_page] = false;
        }
        mod.info("RAC2: captured custom editor page " + std::to_string(editor_page + 1));
    }

    void end_editor(const openrac_play_status& status, bool before_exit) {
        if (!editor_active) return;
        if (mod.loaded_level() >= 0 && status.loads == editor_loads) {
            if (before_exit) capture_editor();
            if (editor_page != 0) {
                const auto primary = banks.slots(Page::Primary);
                // Restore primary before the native exit callback syncs
                // cache -> save slots; also repair a late exit on frame.
                if (live_cache() == editor_cache) write_editor_slots(primary);
                mod.memory().write(mod.relocate(kNativeSlots), primary);
            }
        }
        editor_active = false;
        editor_cache = 0;
        editor_page = 0;
        visible_page.store(0, std::memory_order_relaxed);
        mod.info("RAC2: left Quick Select editor; primary restored");
    }

    bool handle_editor(const openrac_play_status& status, std::uint16_t held,
                       std::uint8_t* report, int size, bool pad) {
        const bool in_editor = editor_context(status);
        if (!in_editor) {
            if (editor_active) end_editor(status, false);
            editor_exiting = false;
            if (pad) editor_previous = held;
            return false;
        }
        if (editor_exiting) return true; // wait for the native page to change
        if (!editor_active) {
            cancel_wheel(last_l1, last_r1);
            refresh_slot();
            editor_cache = live_cache();
            Slots initial{};
            if (!read_editor_slots(initial) ||
                initial != mod.memory().read<Slots>(mod.relocate(kNativeSlots))) {
                if (!editor_warned) {
                    mod.warn("RAC2: editor cache not initialised/matched; leaving native editor alone");
                    editor_warned = true;
                }
                return true;
            }
            editor_warned = false;
            editor_active = true;
            editor_loads = status.loads;
            editor_page = 0;
            banks.set_primary(initial);
            editor_previous = held;
            mod.info("RAC2: native Quick Select editor entered");
        }
        if (status.loads != editor_loads || live_cache() != editor_cache) {
            end_editor(status, false);
            return true;
        }
        capture_editor();
        if (pad) {
            using namespace rac2_input;
            const auto pressed = static_cast<std::uint16_t>(held & ~editor_previous);
            editor_previous = held;
            if (pressed & kEditorExit) {
                end_editor(status, true);
                editor_exiting = true;
                return true; // native back/confirm still runs
            }
            const bool left = (pressed & kL2) != 0, right = (pressed & kR2) != 0;
            if (left != right && banks.page_count() > 1) {
                const auto count = banks.page_count();
                const auto next = (editor_page + (left ? count - 1 : 1)) % count;
                auto slots = banks.slots(page_at(next));
                if (write_editor_slots(slots)) {
                    editor_page = next;
                    mod.info("RAC2: editor page " + std::to_string(next + 1));
                }
            }
            if (pressed & kSquare) {
                const auto mem = mod.memory();
                const auto base = mem.read<openrac::gaddr>(mod.relocate(kResidentBase));
                if (guest_range(base, kEditorObjectOffset + kEditorCursorOffset + 4)) {
                    const auto cursor = mem.read<std::int32_t>(
                        base + kEditorObjectOffset + kEditorCursorOffset);
                    Slots slots{};
                    if (cursor >= 0 && cursor < 8 && read_editor_slots(slots)) {
                        // Native assignment targets cursor+1 (highlight 0
                        // denotes the slot at top-right, gameplay slot 1).
                        slots[static_cast<std::size_t>((cursor + 1) % 8)] = 0;
                        if (write_editor_slots(slots)) capture_editor();
                    }
                }
            }
            consume(report, size, kL2 | kR2 | kSquare);
        }
        visible_page.store(-static_cast<int>(editor_page + 1), std::memory_order_relaxed);
        return true;
    }

    std::uint32_t close_stamp() const {
        return mod.memory().read<std::uint32_t>(mod.relocate(kWheelCloseFrame));
    }

    // Keep the last visible extra page published during the native closing
    // animation. The menu's pending argument can become 0 *before* the
    // selector's fade reaches zero and it reads the chosen item.
    // Do not let a generic phase/argument transition cancel it early.
    bool handle_close(const openrac_play_status& status, bool triangle, bool frame) {
        if (!closing && (!native_seen || !banks.wheel_open() ||
                         banks.active_page() == Page::Primary)) return false;
        const bool same_level = mod.loaded_level() >= 0 && status.loads == native_loads;
        if (!same_level) {
            if (!closing) return false;
            restore();
            cancel_wheel(last_l1, last_r1);
            return true;
        }
        const auto mem = mod.memory();
        const auto state = mem.read<std::int32_t>(mod.relocate(kGameState));
        const auto arg = mem.read<std::int32_t>(mod.relocate(kStateArg));
        const bool wheel_or_fade = rac2_close::context(
            status.phase == OPENRAC_PLAY_MENU,
            status.phase == OPENRAC_PLAY_PLAYING, state, arg);
        const auto stamp = close_stamp();
        if (!closing) {
            if (triangle && arg == kQuickSelectArg) return false;
            if (!wheel_or_fade || stamp != native_close_stamp) return false;
            closing = true;
            closing_frames = 0;
            logged_resume = false;
            mod.info("RAC2: retaining page " +
                     std::to_string(page_index(banks.active_page()) + 1) +
                     " until native wheel commit; close stamp " + std::to_string(stamp));
        }
        const bool committed = stamp != native_close_stamp;
        if (state == 0 && arg == 0 && !logged_resume) {
            logged_resume = true;
            mod.info("RAC2: gameplay resumed before native wheel close; retaining extra page");
        }
        if (frame) ++closing_frames;
        if (committed || !wheel_or_fade || closing_frames >= kMaxClosingFrames) {
            if (committed) {
                const auto selected = mem.read<std::int32_t>(mod.relocate(kWheelClosedSelection));
                const auto& page = banks.slots(banks.active_page());
                const auto expected = selected >= 0 && selected < static_cast<int>(page.size())
                    ? page[static_cast<std::size_t>(selected)] : 0;
                mod.info("RAC2: native wheel committed slot " + std::to_string(selected) +
                         " on page " + std::to_string(page_index(banks.active_page()) + 1) +
                         "; expected item " + std::to_string(expected) +
                         "; pending item " +
                         std::to_string(mem.read<std::int32_t>(mod.relocate(kPendingItem))) +
                         "; equipped item " +
                         std::to_string(mem.read<std::int32_t>(mod.relocate(kEquippedItem))) +
                         "; closing frames " + std::to_string(closing_frames));
            } else {
                mod.warn("RAC2: closing projection ended without observed commit; state=" +
                         std::to_string(state) + " arg=" + std::to_string(arg) +
                         " frames=" + std::to_string(closing_frames));
            }
            restore();
            cancel_wheel(last_l1, last_r1);
            return true;
        }
        // Unlike the held-wheel path, keep the projection live across
        // frame callbacks: the game's selection may happen on a later tick
        // even if no further pad report is received.
        if (!projection_active) {
            const auto primary = mem.read<Slots>(mod.relocate(kNativeSlots));
            auto page = banks.slots(banks.active_page());
            const auto owned = mem.read<rac2_inventory::Owned>(mod.relocate(kInventoryOwned));
            for (auto& item : page)
                if (!rac2_inventory::is_owned(owned, item)) item = 0;
            if (!valid_game_slots(primary, kRac2MaxItemId) ||
                !project(primary, page, status.loads)) {
                mod.warn("RAC2: unable to retain closing projection");
                cancel_wheel(last_l1, last_r1);
                return true;
            }
        }
        last_triangle = triangle;
        return true;
    }

    bool native_wheel_menu(const openrac_play_status& status) const {
        if (mod.loaded_level() < 0 || status.phase != OPENRAC_PLAY_MENU) return false;
        const auto mem = mod.memory();
        return mem.read<std::int32_t>(mod.relocate(kGameState)) == kDialogState &&
               mem.read<std::int32_t>(mod.relocate(kStateArg)) == kQuickSelectArg;
    }

    bool wheel_phase(const openrac_play_status& status) {
        if (mod.loaded_level() < 0) return false;
        if (status.phase == OPENRAC_PLAY_PLAYING) {
            last_game_state = last_state_arg = -1;
            return true;
        }
        if (status.phase == OPENRAC_PLAY_MENU) {
            const auto mem = mod.memory();
            const auto state = mem.read<std::int32_t>(mod.relocate(kGameState));
            const auto arg = mem.read<std::int32_t>(mod.relocate(kStateArg));
            const bool quick = state == kDialogState && arg == kQuickSelectArg;
            if (quick && !native_seen) {
                native_seen = true;
                native_loads = status.loads;
                native_close_stamp = close_stamp();
            }
            if ((banks.wheel_open() || quick) &&
                (state != last_game_state || arg != last_state_arg)) {
                last_game_state = state;
                last_state_arg = arg;
                mod.info("RAC2: menu transition state=" + std::to_string(state) +
                         " arg=" + std::to_string(arg) +
                         (quick ? " (Quick Select)" : " (not Quick Select)"));
            }
            return quick;
        }
        return false;
    }

    void cancel_wheel(bool l1, bool r1) {
        closing = native_seen = false;
        closing_frames = 0;
        banks.close_wheel();
        wheel.before_update(banks, false, l1, r1);
        last_triangle = false;
        visible_page.store(0, std::memory_order_relaxed);
        last_game_state = last_state_arg = -1;
    }

    void sync_inventory() {
        if (banks.page_count() < 2) return;
        const auto mem = mod.memory();
        const auto primary = mem.read<Slots>(mod.relocate(kNativeSlots));
        if (!valid_game_slots(primary, kRac2MaxItemId)) return;
        banks.set_primary(primary);
        const auto order = mem.read<rac2_inventory::Order>(mod.relocate(kInventoryOrder));
        const auto owned = mem.read<rac2_inventory::Owned>(mod.relocate(kInventoryOwned));
        // The native list has six-bit IDs and ownership flags. Exclude
        // entries the native ring editor would reject (no icon / 0xEA7E).
        auto eligible = owned;
        for (std::int32_t id = 1; id <= kRac2MaxItemId; ++id) {
            if (!rac2_inventory::is_owned(owned, id)) continue;
            const auto icon = icon_for(id);
            if (icon == 0 || icon == 0xEA7E) eligible[static_cast<std::size_t>(id)] = 0;
        }
        if (!logged_inventory && std::any_of(primary.begin(), primary.end(),
                                              [](auto item) { return item != 0; })) {
            std::string ids;
            for (auto entry : order) {
                const auto id = rac2_inventory::order_item(entry);
                if (rac2_inventory::is_owned(eligible, id))
                    ids += " " + std::to_string(id);
            }
            if (!ids.empty() || !warned_inventory)
                mod.info("RAC2: eligible owned list IDs:" + (ids.empty() ? std::string(" (none)") : ids));
            // Retry logging when the inventory becomes available after loading.
            logged_inventory = !ids.empty();
            if (!logged_inventory && !warned_inventory) {
                warned_inventory = true;
                std::string raw;
                for (auto entry : order) raw += " " + std::to_string(entry);
                mod.info("RAC2: initial raw inventory list:" + raw);
            }
        }
        const auto changed = rac2_inventory::fill_owned_pages(banks, order, eligible, customised);
        for (std::size_t p = 1; p < banks.page_count(); ++p) {
            if (changed[p]) {
                dirty_pages[p] = true;
                const auto& slots = banks.slots(page_at(p));
                mod.info("RAC2: populated wheel " + std::to_string(p + 1) + " with " +
                         std::to_string(std::count_if(slots.begin(), slots.end(),
                             [](auto item) { return item != 0; })) + " items");
            }
            if (!dirty_pages[p] || last_save_slot < 0) continue;
            auto& destination = customised[p] ? custom_store : store;
            if (destination.save(last_save_slot, page_at(p), banks.slots(page_at(p)))) {
                dirty_pages[p] = false;
                mod.info("RAC2: saved populated extra wheel " + std::to_string(p + 1));
            } else if (!warned_store) {
                warned_store = true;
                mod.warn("RAC2: could not save extra wheel; will retry");
            }
        }
    }

    void refresh_slot() {
        const int host_slot = mod.save_slot_current();
        const int slot = host_slot >= 0 ? host_slot : manual_profile;
        if (slot == last_save_slot) return;
        last_save_slot = slot;
        dirty_pages.fill(false);
        customised.fill(false);
        warned_store = false;
        warned_cache = false;
        logged_projection = false;
        logged_inventory = warned_inventory = false;
        closing = native_seen = false;
        closing_frames = 0;
        wheel = WheelSession{};
        for (std::size_t p = 1; p < kMaxPages; ++p) {
            const auto custom = custom_store.load(slot, page_at(p));
            customised[p] = custom.has_value();
            banks.set_page(page_at(p), custom.value_or(store.load(slot, page_at(p)).value_or(Slots{})));
        }
        banks.close_wheel();
        visible_page.store(0, std::memory_order_relaxed);
        if (slot < 0) mod.info("RAC2: no save slot reported by host; extra pages are session-only");
        else if (host_slot < 0) mod.info("RAC2: loaded manual layout profile " + std::to_string(slot - 99));
        else mod.info("RAC2: loaded extra wheels for save slot " + std::to_string(slot));
    }

    static bool guest_range(openrac::gaddr addr, std::size_t size) {
        return addr >= 0x10000 && addr < kGuestRamSize &&
               size <= kGuestRamSize - addr;
    }

    // These constants come from the executable, NOT the loaded planet.
    // relocate_level(current, address) returns the input unchanged.
    // Resolve the actual ring through the same pointer chain as its
    // native update/draw code: pages[page_index].
    openrac::gaddr live_cache() const {
        const auto mem = mod.memory();
        const auto count_addr = mod.relocate(kRadialSlotCount);
        const auto index_addr = mod.relocate(kRadialPageIndex);
        const auto pages_addr = mod.relocate(kRadialPages);
        if (!guest_range(count_addr, 4) || !guest_range(index_addr, 4) ||
            !guest_range(pages_addr, 4)) return 0;
        if (mem.read<std::int32_t>(count_addr) != 8 ||
            mem.read<std::int32_t>(index_addr) != 0) return 0;
        const auto pages = mem.read<openrac::gaddr>(pages_addr);
        if (!guest_range(pages, 4) || pages % 4 != 0) return 0;
        const auto cache = mem.read<openrac::gaddr>(pages);
        return guest_range(cache, sizeof(Cache)) && cache % 4 == 0 ? cache : 0;
    }

    // Original weapon_menu_init_slots uses executable g_weapon_table,
    // indexed by g_item_slot_to_record. Use executable-to-active relocation.
    std::uint32_t icon_for(std::int32_t item) const {
        const auto mem = mod.memory();
        const auto map = mod.relocate(kItemMap);
        const auto records = mod.relocate(kItemRecords);
        if (item < 0 || item > kRac2MaxItemId ||
            !guest_range(map, kRac2MaxItemId + 1)) return 0;
        const auto mapping = mem.read<std::uint8_t>(map + static_cast<openrac::gaddr>(item));
        if (!guest_range(records, (static_cast<std::size_t>(mapping) + 1) * kRecordStride))
            return 0;
        return mem.read<std::uint16_t>(records + mapping * kRecordStride + 0x3C);
    }

    bool project(const Slots& primary, const Slots& secondary, std::uint32_t loads) {
        if (!valid_game_slots(secondary, kRac2MaxItemId)) return false;
        const auto mem = mod.memory();
        const auto cache_addr = live_cache();
        if (!cache_addr) {
            if (!warned_cache) {
                warned_cache = true;
                mod.warn("RAC2: native radial page unavailable; refusing projection");
            }
            return false;
        }
        const auto cache = mem.read<Cache>(cache_addr);
        // Require a native ring that matches the canonical page and item
        // records before writing anything. Never write to an unverified pointer.
        bool good = false;
        for (std::size_t i = 0; i < primary.size(); ++i) {
            const auto expected = icon_for(primary[i]);
            if (cache[i].item != primary[i] ||
                cache[i].icon != expected || (primary[i] != 0 && expected == 0)) {
                if (!warned_cache) {
                    mod.warn("RAC2 HUD cache mismatch at slot " + std::to_string(i) +
                             ": native item " + std::to_string(primary[i]) +
                             ", cache item " + std::to_string(cache[i].item) +
                             ", cache icon " + std::to_string(cache[i].icon) +
                             ", expected icon " + std::to_string(expected) +
                             ", live cache " + std::to_string(cache_addr) +
                             ", relocated cache " + std::to_string(mod.relocate(kHudCache)) +
                             ", records " + std::to_string(mod.relocate(kItemRecords)) +
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
            if (secondary[i] != 0 && replacement[i].icon == 0) {
                if (!warned_cache) {
                    warned_cache = true;
                    mod.warn("RAC2: missing icon for extra item " + std::to_string(secondary[i]));
                }
                return false;
            }
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
        if (warned_cache || !logged_projection) {
            mod.info("RAC2: native HUD cache matched at " + std::to_string(cache_addr) +
                     "; extra pages available");
            warned_cache = false;
            logged_projection = true;
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
        if (mod.loaded_level() < 0 || mod.play_status().loads != projected_loads) return;
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
    SlotStore custom_store;
    Banks banks;
    int manual_profile = -1;
    bool editor_active = false, editor_exiting = false, editor_warned = false;
    std::size_t editor_page = 0;
    std::uint16_t editor_previous = 0;
    std::uint32_t editor_loads = 0;
    openrac::gaddr editor_cache = 0, last_editor_source = 0;
    rac2_inventory::Changed customised{};
    WheelSession wheel;
    int last_save_slot = -2;
    int last_game_state = -1, last_state_arg = -1;
    std::atomic<int> visible_page{0};
    bool last_triangle = false;
    bool last_l1 = false, last_r1 = false;
    bool warned_cache = false;
    bool logged_projection = false;
    bool logged_inventory = false, warned_inventory = false;
    bool projection_active = false;
    bool native_seen = false, closing = false, logged_resume = false;
    std::uint32_t native_loads = 0, native_close_stamp = 0;
    unsigned closing_frames = 0;
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
    if (!mod.has_overlay_phases())
        throw openrac::Error("RAC2 dev build requires OpenRAC mod API 1.31+");
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
    if (!mod.on_overlay_phases(
            [](void* user, const openrac_overlay* overlay) {
                static_cast<Runtime*>(user)->draw_overlay(overlay);
            }, &r, OPENRAC_OVERLAY_PLAYING | OPENRAC_OVERLAY_MENU))
        throw openrac::Error("RAC2 menu overlay registration refused");
    mod.hook<openrac::rac2::Item_Give>(
        [&r](auto original, std::int32_t item) {
            r.before_item_give();
            original(item);
        });
    mod.warn("RAC2 NTSC-U Quick Select gameplay tested; native pause editor experimental");
}
} // namespace expanded_quick_select
