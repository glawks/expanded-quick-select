#include "expanded_quick_select/banks.hpp"
#include "expanded_quick_select/rac2_input.hpp"
#include "expanded_quick_select/rac2_close.hpp"
#include "expanded_quick_select/rac2_inventory.hpp"
#include <iterator>
#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/slot_store.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
    namespace eqs = expanded_quick_select;
    namespace close = eqs::rac2_close;
    static_assert(close::context(true, false, 4, 7));
    static_assert(close::context(true, false, 4, 0));
    // Observed dev.7 failure: gameplay resumes after 3 frames, before commit.
    static_assert(close::context(false, true, 0, 0));
    static_assert(close::context(true, false, 0, 0));
    static_assert(!close::context(true, false, 4, 3));
    static_assert(!close::context(false, false, 0, 0));
    static_assert(!close::context(false, true, 2, 0));
    namespace ri = expanded_quick_select::rac2_input;
    std::uint8_t report[] = {0xFF, 0xE3, 0x80, 0x80}; // Triangle, L1 and R1 held
    assert((ri::held(report, 4) & (ri::kTriangle | ri::kShoulders)) ==
           (ri::kTriangle | ri::kShoulders));
    ri::consume_page_buttons(report, 4);
    assert((ri::held(report, 4) & ri::kShoulders) == 0);
    assert((ri::held(report, 4) & ri::kTriangle) != 0);
    assert(report[2] == 0x80 && report[3] == 0x80);
    assert(ri::held(nullptr, 0) == 0);
    ri::consume_page_buttons(nullptr, 0);

    eqs::Banks rac1;
    eqs::Banks rac2({}, 55);
    assert(!rac1.set_slot(eqs::Page::Secondary, 0, 55));
    assert(rac2.set_slot(eqs::Page::Secondary, 0, 55));
    assert(!rac2.set_slot(eqs::Page::Secondary, 0, 56));
    eqs::Slots valid{{40, 41, 42, 43, 44, 45, 54, 55}};
    assert(!eqs::valid_game_slots(valid));
    assert(eqs::valid_game_slots(valid, 55));
    valid[0] = 56;
    assert(!eqs::valid_game_slots(valid, 55));

    namespace inv = eqs::rac2_inventory;
    eqs::Banks existing({{1, 2, 3, 4, 5, 6, 7, 8}}, 55);
    existing.set_page_count(3);
    existing.set_slot(eqs::Page::Secondary, 1, 12);
    inv::Owned owned{};
    owned.fill(1);
    owned[20] = 0;
    inv::Order order{};
    order.fill(0xFF);
    assert(inv::order_item(0x5E) == 30); // native owned starting item
    assert(inv::order_item(0x6A) == 42);
    assert(inv::order_item(0xC9) == 9);
    assert(inv::order_item(0xFF) == 0);
    // Real owned entries carry 0x40; mixed flags, unowned IDs and
    // unused entries must not break automatic population.
    const std::uint8_t entries[] = {0x41, 0x49, 0xC9, 0x4C, 0x4A, 0x54, 127, 0,
                                    0x4B, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x77};
    std::copy(std::begin(entries), std::end(entries), order.begin());
    auto changes = inv::fill_owned_pages(existing, order, owned);
    assert(!changes[0] && changes[1] && changes[2]);
    const eqs::Slots expected_primary{{1, 2, 3, 4, 5, 6, 7, 8}};
    const eqs::Slots expected_secondary{{9, 12, 10, 11, 13, 14, 15, 16}};
    assert(existing.slots(eqs::Page::Primary) == expected_primary);
    assert(existing.slots(eqs::Page::Secondary) == expected_secondary);
    assert(existing.slots(eqs::Page::Third)[0] == 17);
    assert(existing.slots(eqs::Page::Third)[1] == 55);
    assert(inv::fill_owned_pages(existing, order, owned) == inv::Changed{});
    assert(!inv::is_owned(owned, 0) && !inv::is_owned(owned, 56) &&
           !inv::is_owned(owned, -1) && !inv::is_owned(owned, 20));
    existing.set_page_count(1);
    assert(inv::fill_owned_pages(existing, order, owned) == inv::Changed{});
    assert(existing.slots(eqs::Page::Secondary) == expected_secondary);

    // A manually cleared slot must remain empty; other pages still auto-fill.
    existing.set_page_count(3);
    existing.set_slot(eqs::Page::Secondary, 0, 0);
    inv::Changed customised{};
    customised[1] = true;
    inv::fill_owned_pages(existing, order, owned, customised);
    assert(existing.slots(eqs::Page::Secondary)[0] == 0);
    std::uint8_t editor_report[] = {0xFF, 0x70, 0x80, 0x80};
    ri::consume(editor_report, 4, ri::kL2 | ri::kR2 | ri::kSquare);
    assert((ri::held(editor_report, 4) & (ri::kL2 | ri::kR2 | ri::kSquare)) == 0);
    assert((ri::held(editor_report, 4) & ri::kShoulders) == ri::kShoulders);

    auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() / ("eqs-rac2-" + std::to_string(suffix));
    eqs::SlotStore store(dir, 55);
    valid[0] = 40;
    assert(store.save(0, valid));
    assert(store.load(0).value() == valid);
    eqs::SlotStore rac1_store(dir);
    assert(!rac1_store.load(0).has_value()); // RAC1 rejects RAC2's higher IDs
    std::filesystem::remove_all(dir);
}
