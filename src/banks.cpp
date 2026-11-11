#include "expanded_quick_select/banks.hpp"

namespace expanded_quick_select {

Banks::Banks(Slots primary) : primary_(primary) {}

void Banks::set_primary(const Slots& items) noexcept { primary_ = items; }
void Banks::set_secondary(const Slots& items) noexcept { secondary_ = items; }

const Slots& Banks::slots(Page page) const noexcept {
    return page == Page::Primary ? primary_ : secondary_;
}

bool Banks::set_slot(Page page, std::size_t index, std::int32_t item) noexcept {
    if (index >= Slots{}.size() || item < 0) {
        return false;
    }
    (page == Page::Primary ? primary_ : secondary_)[index] = item;
    return true;
}

Page Banks::active_page() const noexcept { return active_; }

void Banks::open_wheel() noexcept {
    wheel_open_ = true;
    active_ = Page::Primary;
}

void Banks::close_wheel() noexcept { wheel_open_ = false; }

bool Banks::switch_on_press(bool l1_down, bool r1_down) noexcept {
    const bool l1_edge = l1_down && !prev_l1_;
    const bool r1_edge = r1_down && !prev_r1_;
    prev_l1_ = l1_down;
    prev_r1_ = r1_down;

    if (!wheel_open_ || l1_edge == r1_edge) {
        return false;
    }
    // With two pages, either shoulder button wraps to the other page.
    active_ = active_ == Page::Primary ? Page::Secondary : Page::Primary;
    return true;
}

void Banks::reset_for_new_game(const Slots& primary) noexcept {
    primary_ = primary;
    secondary_ = {};
    active_ = Page::Primary;
    wheel_open_ = false;
    prev_l1_ = false;
    prev_r1_ = false;
}

} // namespace expanded_quick_select
