#include "expanded_quick_select/sessions.hpp"
#include "expanded_quick_select/input_filter.hpp"
#include "expanded_quick_select/editor_hint.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string_view>

using namespace expanded_quick_select;

int main() {
    // R1/L1 used to turn wheel pages must not reach hero actions (R1 slam).
    // Gameplay inputs are unchanged as soon as Triangle is released.
    // R1 and R2 both trigger airborne slam in RAC1; neither may do so
    // while Quick Select or its pause editor is active. Outside both,
    // normal shoulder controls must be completely preserved.
    assert(gameplay_shoulder_mask(0x00) == 0);
    assert(gameplay_shoulder_mask(0x02) == 0); // R2 outside wheel: normal slam
    assert(gameplay_shoulder_mask(0x08) == 0); // R1 outside wheel: normal slam
    assert(gameplay_shoulder_mask(0x0F) == 0); // all shoulders, no Triangle
    assert(gameplay_shoulder_mask(0x10) == 0x0F);
    assert(gameplay_shoulder_mask(0x12) == 0x0F); // Triangle+R2
    assert(gameplay_shoulder_mask(0x18) == 0x0F); // Triangle+R1
    assert(gameplay_shoulder_mask(0x14) == 0x0F); // Triangle+L1
    assert(gameplay_shoulder_mask(0x1F) == 0x0F); // Triangle+all shoulders
    assert(gameplay_shoulder_mask(0x02, true) == 0x0F); // pause editor R2
    assert(gameplay_shoulder_mask(0x08, true) == 0x0F); // pause editor R1
    assert(gameplay_shoulder_mask(0x00, false) == 0);
    assert(filtered_gameplay_buttons(0x12, gameplay_shoulder_mask(0x12)) == 0x10);
    assert(filtered_gameplay_buttons(0x18, gameplay_shoulder_mask(0x18)) == 0x10);
    assert(filtered_gameplay_buttons(0x1F, gameplay_shoulder_mask(0x1F)) == 0x10);
    assert(filtered_gameplay_buttons(0x42, gameplay_shoulder_mask(0x02, true)) == 0x40);
    assert(filtered_gameplay_buttons(0x48, gameplay_shoulder_mask(0x48)) == 0x48);
    assert(filtered_gameplay_buttons(0x02, gameplay_shoulder_mask(0x02)) == 0x02);
    assert(filtered_gameplay_buttons(0x08, gameplay_shoulder_mask(0x08)) == 0x08);
    // RAC1's airborne action also reads a 30-frame history of presses.
    // The newest history record must lose R1 when Triangle is held.
    assert(last_history_index(0) == 29);
    assert(last_history_index(1) == 0);
    assert(last_history_index(29) == 28);
    assert(filtered_gameplay_buttons(0x48, gameplay_shoulder_mask(0x18)) == 0x40);
    assert(filtered_gameplay_buttons(0x42, gameplay_shoulder_mask(0x12)) == 0x40);
    assert(filtered_gameplay_buttons(0x48, gameplay_shoulder_mask(0x08)) == 0x48);
    assert(filtered_gameplay_buttons(0x42, gameplay_shoulder_mask(0x02)) == 0x42);

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

    assert(help_message_pointer_address(true) == 0x0015F6A0u);
    assert(help_message_pointer_address(false) == 0x0015F780u);
    assert(std::string_view(editor_page_hint(Page::Primary)) ==
           "L2   PAGE 1 / 2   R2");
    assert(std::string_view(editor_page_hint(Page::Secondary)) ==
           "L2   PAGE 2 / 2   R2");
    assert(editor_label_scroll_16ths(200) == -172 * 16);
    assert(editor_label_scroll_16ths(32) == 0);
    assert(editor_page_hint(Page::Third, 4) == "L2   PAGE 3 / 4   R2");
    assert(editor_page_hint(Page::Eighth, 8) == "L2   PAGE 8 / 8   R2");

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

    // With 4 wheels, editor L2 goes backward and R2 forward, wrapping.
    Banks expanded(first);
    expanded.set_page_count(4);
    expanded.set_page(Page::Fourth, second);
    editor.begin(expanded);
    assert(editor.switch_page(expanded, true, false, first));
    assert(editor.active_page() == Page::Fourth);
    assert(editor.visible_slots(expanded) == second);
    const Slots revised_fourth{{30, 31, 32, 33, 34, 35, 36, 37}};
    assert(editor.switch_page(expanded, false, true, revised_fourth));
    assert(expanded.slots(Page::Fourth) == revised_fourth);
    assert(editor.active_page() == Page::Primary);
    assert(editor.switch_page(expanded, false, true, first));
    assert(editor.active_page() == Page::Secondary);
    assert(editor.switch_page(expanded, false, true, second));
    assert(editor.active_page() == Page::Third);
    assert(editor.switch_page(expanded, false, true, Slots{}));
    assert(editor.active_page() == Page::Fourth);
    assert(editor_page_hint(editor.active_page(), expanded.page_count()) ==
           "L2   PAGE 4 / 4   R2");
    editor.end();
    expanded.set_page_count(1);
    editor.begin(expanded);
    assert(!editor.switch_page(expanded, false, true, first));
    assert(editor.active_page() == Page::Primary);
    editor.end();

    assert(valid_game_slots(first));
    assert(!valid_game_slots(Slots{{1, 2, 3, 4, 40, 6, 7, 8}}));
    assert(!valid_game_slots(Slots{{1, 2, 3, 4, -1, 6, 7, 8}}));
}
