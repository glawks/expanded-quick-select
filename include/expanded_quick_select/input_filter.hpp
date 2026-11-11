#pragma once
#include <cstdint>

namespace expanded_quick_select {

// Shoulder buttons are wheel navigation while Triangle is held; prevent the
// hero movement/item logic from handling them as attacks/Thruster-Pack actions.
// The wheel itself must continue reading held_unmasked, not these filtered bits.
constexpr std::uint32_t gameplay_shoulder_mask(std::uint32_t held_unmasked) noexcept {
    constexpr std::uint32_t kTriangle = 0x10;
    constexpr std::uint32_t kShoulders = 0x04 | 0x08;
    return (held_unmasked & kTriangle) != 0 ? kShoulders : 0u;
}

constexpr std::uint32_t filtered_gameplay_buttons(std::uint32_t buttons,
                                                  std::uint32_t mask) noexcept {
    return buttons & ~mask;
}
} // namespace expanded_quick_select
