#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/input_filter.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

using namespace expanded_quick_select;

int main() {
    // R1/L1 used to turn wheel pages must not reach hero actions (R1 slam).
    // Gameplay inputs are unchanged as soon as Triangle is released.
    assert(gameplay_shoulder_mask(0x00) == 0);
    assert(gameplay_shoulder_mask(0x08) == 0); // R1 without Triangle: normal slam
    assert(gameplay_shoulder_mask(0x10) == 0x0C);
    assert(gameplay_shoulder_mask(0x18) == 0x0C);
    assert(gameplay_shoulder_mask(0x14) == 0x0C);
    assert(filtered_gameplay_buttons(0x18, gameplay_shoulder_mask(0x18)) == 0x10);
    assert(filtered_gameplay_buttons(0x1C, gameplay_shoulder_mask(0x1C)) == 0x10);
    assert(filtered_gameplay_buttons(0x48, gameplay_shoulder_mask(0x48)) == 0x48);
    assert(filtered_gameplay_buttons(0x08, gameplay_shoulder_mask(0x08)) == 0x08);
    // RAC1's airborne action also reads a 30-frame history of presses.
    // The newest history record must lose R1 when Triangle is held.
    assert(last_history_index(0) == 29);
    assert(last_history_index(1) == 0);
    assert(last_history_index(29) == 28);
    assert(filtered_gameplay_buttons(0x48, gameplay_shoulder_mask(0x18)) == 0x40);
    assert(filtered_gameplay_buttons(0x48, gameplay_shoulder_mask(0x08)) == 0x48);

    const Slots first{{1, 2, 3, 4, 5, 6, 7, 8}};
    const Slots second{{9, 10, 11, 12, 13, 14, 15, 16}};
    Banks banks(first);
    banks.set_secondary(second);
    WheelSession wheel;

    // Outside Triangle, shoulder presses must not open or flip the wheel.
    assert(!wheel.before_update(banks, false, false, true));
    wheel.after_update(banks, false);
    assert(banks.active_page() == Page::Primary);
    assert(wheel.before_update(banks, true, false, true));
    assert(banks.active_page() == Page::Primary); // pre-held R1 is not a fresh press
    assert(wheel.before_update(banks, true, false, false));
    assert(wheel.before_update(banks, true, false, true));
    assert(banks.active_page() == Page::Secondary);
    assert(wheel.before_update(banks, true, false, true));
    assert(banks.active_page() == Page::Secondary); // no auto-repeat

    // The release frame must still use page 2 to equip that page's item.
    assert(!wheel.before_update(banks, false, false, false));
    assert(banks.active_page() == Page::Secondary);
    wheel.after_update(banks, false);
    assert(wheel.before_update(banks, true, false, false));
    assert(banks.active_page() == Page::Primary); // fresh wheel begins at page 1

    EditorSession editor;
    editor.begin(banks);
    assert(editor.active_page() == Page::Primary);
    assert(editor.visible_slots(banks) == first);

    // L2/R2 changes editor bank, while L1/R1 remain the game's slot-navigation keys.
    const Slots revised_first{{1, 2, 3, 0, 5, 6, 7, 8}};
    assert(editor.switch_page(banks, true, false, revised_first));
    assert(banks.slots(Page::Primary) == revised_first);
    assert(editor.active_page() == Page::Secondary);
    assert(editor.visible_slots(banks) == second);
    const Slots revised_second{{0, 10, 11, 12, 13, 14, 15, 16}};
    editor.capture(banks, revised_second);
    assert(banks.slots(Page::Secondary) == revised_second);
    assert(banks.slots(Page::Primary) == revised_first);
    assert(!editor.switch_page(banks, true, true, revised_second));
    assert(editor.switch_page(banks, false, true, revised_second));
    assert(editor.active_page() == Page::Primary);
    editor.end();
    assert(!editor.active());

    assert(valid_game_slots(first));
    assert(!valid_game_slots(Slots{{1, 2, 3, 4, 40, 6, 7, 8}}));
    assert(!valid_game_slots(Slots{{1, 2, 3, 4, -1, 6, 7, 8}}));
}
