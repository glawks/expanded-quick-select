#pragma once
#include <cstdint>

namespace expanded_quick_select {

// RAC1 reads both R1 and R2 for airborne slam/crouch actions. Reserve all
// four shoulder buttons for Quick Select while Triangle is held. The editor
// also uses all four (L1/R1 for slots; L2/R2 for pages).
//
// Only the gameplay-facing held/pressed fields and recent-press history are
// filtered: *_unmasked remains intact for the wheel and the pause editor.
// Outside these two contexts no controller bits are removed.
constexpr std::uint32_t gameplay_shoulder_mask(std::uint32_t held_unmasked,
                                               bool editor_active = false) noexcept {
    constexpr std::uint32_t kTriangle = 0x10;
    constexpr std::uint32_t kShoulders = 0x01 | 0x02 | 0x04 | 0x08;
    return editor_active || (held_unmasked & kTriangle) != 0 ? kShoulders : 0u;
}

constexpr std::uint32_t filtered_gameplay_buttons(std::uint32_t buttons,
                                                  std::uint32_t mask) noexcept {
    return buttons & ~mask;
}
// Pad_ProcessReport records the current frame at hist_btn[hist_idx] and then
// advances hist_idx. This returns the index of that just-recorded frame.
constexpr std::uint32_t last_history_index(std::uint32_t next_index) noexcept {
    return (next_index + 29u) % 30u;
}
} // namespace expanded_quick_select
