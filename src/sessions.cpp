#include "expanded_quick_select/sessions.hpp"

namespace expanded_quick_select {

bool WheelSession::before_update(Banks& banks, bool triangle, bool l1,
                                 bool r1) noexcept {
    if (triangle && !previous_triangle_) banks.open_wheel();
    banks.switch_on_press(l1, r1); // track shoulders even outside the wheel
    previous_triangle_ = triangle;
    return triangle;
}

void WheelSession::after_update(Banks& banks, bool triangle) noexcept {
    if (!triangle) banks.close_wheel();
}

void EditorSession::begin(Banks&) noexcept {
    active_ = true;
    page_ = Page::Primary;
}

void EditorSession::end() noexcept { active_ = false; }

bool EditorSession::active() const noexcept { return active_; }
Page EditorSession::active_page() const noexcept { return page_; }

const Slots& EditorSession::visible_slots(const Banks& banks) const noexcept {
    return banks.slots(page_);
}

void EditorSession::capture(Banks& banks, const Slots& slots) noexcept {
    if (!active_ || !valid_game_slots(slots)) return;
    if (page_ == Page::Primary) banks.set_primary(slots);
    else banks.set_secondary(slots);
}

bool EditorSession::switch_page(Banks& banks, bool l2_pressed,
                                bool r2_pressed, const Slots& visible) noexcept {
    if (!active_ || l2_pressed == r2_pressed) return false;
    capture(banks, visible);
    page_ = page_ == Page::Primary ? Page::Secondary : Page::Primary;
    return true;
}

bool valid_game_slots(const Slots& items) noexcept {
    for (const auto item : items) {
        if (item < 0 || item > 39) return false;
    }
    return true;
}

} // namespace expanded_quick_select
