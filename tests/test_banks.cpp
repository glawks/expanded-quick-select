#include "expanded_quick_select/banks.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

using expanded_quick_select::Banks;
using expanded_quick_select::Page;
using expanded_quick_select::Slots;

int main() {
    const Slots original{{1, 2, 3, 4, 5, 6, 7, 8}};
    Banks banks(original);

    // The secondary page must start empty while the primary stays untouched.
    assert(banks.slots(Page::Primary) == original);
    assert(banks.slots(Page::Secondary) == Slots{});

    // Shoulder buttons do nothing outside the wheel.
    assert(!banks.switch_on_press(true, false));
    assert(banks.active_page() == Page::Primary);
    banks.switch_on_press(false, false);

    banks.open_wheel();
    assert(banks.switch_on_press(false, true));
    assert(banks.active_page() == Page::Secondary);
    assert(!banks.switch_on_press(false, true)); // held R1 cannot flip back
    assert(banks.active_page() == Page::Secondary);
    banks.switch_on_press(false, false);
    assert(banks.switch_on_press(true, false)); // L1 wraps 2 -> 1
    assert(banks.active_page() == Page::Primary);
    banks.switch_on_press(false, false);
    assert(banks.switch_on_press(true, false)); // L1 wraps 1 -> 2
    assert(banks.active_page() == Page::Secondary);

    // Simultaneous new presses are ambiguous and should be a no-op.
    banks.switch_on_press(false, false);
    assert(!banks.switch_on_press(true, true));
    assert(banks.active_page() == Page::Secondary);
    assert(!banks.switch_on_press(true, false)); // releasing R1 isn't a new press
    banks.switch_on_press(false, false);

    // Editing the secondary bank must never modify the primary bank.
    assert(banks.set_slot(Page::Secondary, 3, 21));
    assert(banks.slots(Page::Secondary)[3] == 21);
    assert(banks.slots(Page::Primary) == original);
    assert(!banks.set_slot(Page::Secondary, 8, 99));
    assert(!banks.set_slot(Page::Secondary, 0, -10));
    assert(banks.slots(Page::Secondary)[0] == 0);

    banks.close_wheel();
    assert(!banks.switch_on_press(false, true));
    banks.open_wheel();
    assert(banks.active_page() == Page::Primary);
    assert(banks.slots(Page::Secondary)[3] == 21);

    // Starting a new game clears stale assignments from both banks.
    const Slots new_game{{4, 0, 0, 9, 0, 0, 0, 0}};
    banks.reset_for_new_game(new_game);
    assert(banks.slots(Page::Primary) == new_game);
    assert(banks.slots(Page::Secondary) == Slots{});
    assert(banks.active_page() == Page::Primary);

    // Wheel count setting: 1 disables page switching, 3+ steps in direction.
    assert(banks.page_count() == 2); // backwards-compatible default
    banks.set_page_count(1);
    banks.open_wheel();
    banks.switch_on_press(false, false);
    assert(!banks.switch_on_press(false, true));
    assert(banks.active_page() == Page::Primary);
    banks.close_wheel();
    banks.switch_on_press(false, false);

    banks.set_page_count(3);
    assert(banks.page_count() == 3);
    const Slots third{{31, 32, 33, 0, 0, 0, 0, 0}};
    banks.set_page(Page::Third, third);
    banks.open_wheel();
    assert(banks.switch_on_press(false, true));
    assert(banks.active_page() == Page::Secondary);
    assert(!banks.switch_on_press(false, true));
    banks.switch_on_press(false, false);
    assert(banks.switch_on_press(false, true));
    assert(banks.active_page() == Page::Third);
    assert(banks.slots(Page::Third) == third);
    banks.switch_on_press(false, false);
    assert(banks.switch_on_press(false, true)); // wrap 3 -> 1
    assert(banks.active_page() == Page::Primary);
    banks.switch_on_press(false, false);
    assert(banks.switch_on_press(true, false)); // reverse 1 -> 3
    assert(banks.active_page() == Page::Third);
    assert(!banks.switch_on_press(true, false));
    banks.close_wheel();

    // Reducing wheels must preserve hidden page assignments.
    banks.set_page_count(2);
    assert(banks.active_page() == Page::Primary);
    assert(banks.slots(Page::Third) == third);
    banks.set_page_count(3);
    assert(banks.slots(Page::Third) == third);
    banks.set_page_count(8);
    assert(banks.page_count() == 8);
    const Slots eighth{{7, 7, 7, 7, 7, 7, 7, 7}};
    banks.set_page(Page::Eighth, eighth);
    banks.switch_on_press(false, false);
    banks.open_wheel();
    assert(banks.switch_on_press(true, false)); // 1 -> 8
    assert(banks.active_page() == Page::Eighth);
    assert(banks.slots(Page::Eighth) == eighth);
    assert(!banks.set_slot(Page::Eighth, 8, 1));
    assert(!banks.set_slot(Page::Eighth, 0, 40));
    assert(!banks.set_slot(static_cast<Page>(8), 0, 1));
    assert(banks.slots(static_cast<Page>(8)) == Slots{});
    banks.set_page_count(999);
    assert(banks.page_count() == 8);
    banks.set_page_count(0);
    assert(banks.page_count() == 1);
    assert(banks.active_page() == Page::Primary);
}
