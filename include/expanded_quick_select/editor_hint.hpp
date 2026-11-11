#pragma once

#include "expanded_quick_select/banks.hpp"

namespace expanded_quick_select {

// The pause editor uses L2/R2 (the gameplay wheel uses L1/R1).
// The game's own small menu font draws these at the foot of the ring panel.
constexpr const char* editor_page_hint(Page page) noexcept {
    return page == Page::Primary ? "L2   PAGE 1 / 2   R2"
                                 : "L2   PAGE 2 / 2   R2";
}

// The menu font's default inset is 4 px. Put the text's anchor 24 px
// from the bottom of its panel, leaving room for the game's own border.
constexpr int editor_label_scroll_16ths(int panel_height) noexcept {
    return panel_height > 48 ? (28 - panel_height) * 16 : 0;
}

} // namespace expanded_quick_select
