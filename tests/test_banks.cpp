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
}
