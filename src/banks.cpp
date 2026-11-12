#include "expanded_quick_select/banks.hpp"

namespace expanded_quick_select {

Banks::Banks(Slots primary) { pages_[0] = primary; }

void Banks::set_page_count(std::size_t count) noexcept {
    count_ = count < 1 ? 1 : count > kMaxPages ? kMaxPages : count;
    if (page_index(active_) >= count_) active_ = Page::Primary;
}
std::size_t Banks::page_count() const noexcept { return count_; }

void Banks::set_primary(const Slots& items) noexcept { pages_[0] = items; }
void Banks::set_secondary(const Slots& items) noexcept { pages_[1] = items; }
void Banks::set_page(Page page, const Slots& items) noexcept {
    if (page_index(page) < kMaxPages) pages_[page_index(page)] = items;
}
const Slots& Banks::slots(Page page) const noexcept {
    static constexpr Slots empty{};
    return page_index(page) < kMaxPages ? pages_[page_index(page)] : empty;
}

bool Banks::set_slot(Page page, std::size_t index, std::int32_t item) noexcept {
    if (page_index(page) >= kMaxPages || index >= Slots{}.size() || item < 0 || item > 39)
        return false;
    pages_[page_index(page)][index] = item;
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

    if (!wheel_open_ || count_ <= 1 || l1_edge == r1_edge) return false;

    const std::size_t current = page_index(active_);
    active_ = page_at(l1_edge ? (current + count_ - 1) % count_
                              : (current + 1) % count_);
    return true;
}

void Banks::reset_for_new_game(const Slots& primary) noexcept {
    for (auto& page : pages_) page = {};
    pages_[0] = primary;
    active_ = Page::Primary;
    wheel_open_ = false;
    prev_l1_ = false;
    prev_r1_ = false;
}
} // namespace expanded_quick_select
