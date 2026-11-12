#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace expanded_quick_select {

using Slots = std::array<std::int32_t, 8>;

// Page 1 belongs to RAC1's normal save. Page 2 remains the existing
// save-slot-N.dat sidecar; pages 3..8 use separate sidecar files.
inline constexpr std::size_t kMaxPages = 8;
enum class Page : std::uint8_t {
    Primary = 0, Secondary = 1, Third = 2, Fourth = 3,
    Fifth = 4, Sixth = 5, Seventh = 6, Eighth = 7
};
constexpr std::size_t page_index(Page p) noexcept {
    return static_cast<std::size_t>(p);
}
constexpr Page page_at(std::size_t index) noexcept {
    return static_cast<Page>(index);
}

class Banks {
public:
    explicit Banks(Slots primary = {});

    void set_page_count(std::size_t count) noexcept;
    std::size_t page_count() const noexcept;

    void set_primary(const Slots& items) noexcept;
    void set_secondary(const Slots& items) noexcept;
    void set_page(Page page, const Slots& items) noexcept;
    const Slots& slots(Page page) const noexcept;
    bool set_slot(Page page, std::size_t index, std::int32_t item) noexcept;

    Page active_page() const noexcept;
    void open_wheel() noexcept;
    void close_wheel() noexcept;
    // Arguments are held states. L1 steps back, R1 forward, wrapping
    // within the number of wheels enabled by the mod setting.
    bool switch_on_press(bool l1_down, bool r1_down) noexcept;
    void reset_for_new_game(const Slots& primary) noexcept;

private:
    std::array<Slots, kMaxPages> pages_{};
    std::size_t count_ = 2;
    Page active_ = Page::Primary;
    bool wheel_open_ = false;
    bool prev_l1_ = false;
    bool prev_r1_ = false;
};

} // namespace expanded_quick_select
