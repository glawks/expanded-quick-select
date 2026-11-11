#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace expanded_quick_select {

using Slots = std::array<std::int32_t, 8>;

enum class Page : std::uint8_t { Primary, Secondary };

class Banks {
public:
    explicit Banks(Slots primary = {});

    void set_primary(const Slots& items) noexcept;
    void set_secondary(const Slots& items) noexcept;
    const Slots& slots(Page page) const noexcept;
    bool set_slot(Page page, std::size_t index, std::int32_t item) noexcept;

    Page active_page() const noexcept;
    void open_wheel() noexcept;
    void close_wheel() noexcept;
    // Arguments are current button-down states, not already-derived edges.
    // Returns true only for a genuine switch while the wheel is open.
    bool switch_on_press(bool l1_down, bool r1_down) noexcept;
    void reset_for_new_game(const Slots& primary) noexcept;

private:
    Slots primary_{};
    Slots secondary_{};
    Page active_ = Page::Primary;
    bool wheel_open_ = false;
    bool prev_l1_ = false;
    bool prev_r1_ = false;
};

} // namespace expanded_quick_select
