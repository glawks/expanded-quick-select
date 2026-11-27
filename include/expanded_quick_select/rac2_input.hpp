#pragma once
#include <cstddef>
#include <cstdint>

namespace expanded_quick_select::rac2_input {
// OpenRAC on_pad sees PS2 active-low buttons in report bytes 0 and 1,
// not RAC1's internal 32-bit PadState button encoding.
inline constexpr std::uint16_t kL1 = 0x0400;
inline constexpr std::uint16_t kR1 = 0x0800;
inline constexpr std::uint16_t kTriangle = 0x1000;
inline constexpr std::uint16_t kL2 = 0x0100;
inline constexpr std::uint16_t kR2 = 0x0200;
inline constexpr std::uint16_t kSquare = 0x8000;
inline constexpr std::uint16_t kEditorExit = 0x0009 | 0x2000 | kTriangle; // Select, Start, Circle, Triangle
inline constexpr std::uint16_t kShoulders = kL1 | kR1;

inline std::uint16_t held(const std::uint8_t* report, int size) noexcept {
    if (!report || size < 2) return 0;
    return static_cast<std::uint16_t>(~(static_cast<std::uint16_t>(report[0]) |
                                  (static_cast<std::uint16_t>(report[1]) << 8)));
}
inline void consume_page_buttons(std::uint8_t* report, int size) noexcept {
    if (!report || size < 2) return;
    // Setting active-low bits releases L1/R1 for game logic, preserving
    // Triangle and every other input. Page edges were read before this.
    report[1] = static_cast<std::uint8_t>(report[1] | 0x0C);
}
inline void consume(std::uint8_t* report, int size, std::uint16_t mask) noexcept {
    if (!report || size < 2) return;
    report[0] |= static_cast<std::uint8_t>(mask);
    report[1] |= static_cast<std::uint8_t>(mask >> 8);
}
} // namespace expanded_quick_select::rac2_input
