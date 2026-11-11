#pragma once

#include "expanded_quick_select/banks.hpp"

namespace expanded_quick_select {

// The release tick remains on its selected page until the original HUD update
// has equipped the item. A new Triangle press always starts on page one.
class WheelSession {
public:
    bool before_update(Banks& banks, bool triangle, bool l1, bool r1) noexcept;
    void after_update(Banks& banks, bool triangle) noexcept;

private:
    bool previous_triangle_ = false;
};

// The pause editor has its own page; it does not use the gameplay-wheel state.
// Original L1/R1 movement remains available. L2/R2 step editor pages.
class EditorSession {
public:
    void begin(Banks& banks) noexcept;
    void end() noexcept;
    bool active() const noexcept;
    Page active_page() const noexcept;
    const Slots& visible_slots(const Banks& banks) const noexcept;
    void capture(Banks& banks, const Slots& slots) noexcept;
    bool switch_page(Banks& banks, bool l2_pressed, bool r2_pressed,
                     const Slots& currently_visible) noexcept;

private:
    bool active_ = false;
    Page page_ = Page::Primary;
};

// Only valid item IDs may be projected into the game's icon lookup table.
bool valid_game_slots(const Slots& items) noexcept;

} // namespace expanded_quick_select
