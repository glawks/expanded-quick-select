#include "expanded_quick_select/banks.hpp"
#include "expanded_quick_select/rac2_input.hpp"
#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/slot_store.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>

int main() {
    namespace eqs = expanded_quick_select;
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
