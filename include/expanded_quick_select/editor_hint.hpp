#pragma once
#include "expanded_quick_select/banks.hpp"
#include <cstdint>
#include <string>

namespace expanded_quick_select {

// PAL and NTSC-U keep the active message-table pointer at different addresses.
constexpr std::uint32_t help_message_pointer_address(bool ntsc) noexcept {
    return ntsc ? 0x0015F6A0u : 0x0015F780u;
}

// Native-font footer in the pause-menu wheel panel. The actual text is
// generated from the configured count, not hardcoded to two pages.
inline std::string editor_page_hint(Page page, std::size_t count = 2) {
    return "L2   PAGE " + std::to_string(page_index(page) + 1) +
           " / " + std::to_string(count) + "   R2";
}
constexpr int editor_label_scroll_16ths(int panel_height) noexcept {
    return panel_height > 48 ? (28 - panel_height) * 16 : 0;
}
} // namespace expanded_quick_select
