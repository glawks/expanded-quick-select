#pragma once
#include "expanded_quick_select/banks.hpp"
#include <string>

namespace expanded_quick_select {

// Gameplay Quick Select HUD hint. Keep the current page visible even when
// only one wheel is configured, but don't advertise inactive L1/R1 controls.
inline std::string gameplay_page_hint(Page page, std::size_t count) {
    const std::string number = "PAGE " + std::to_string(page_index(page) + 1) +
                               " / " + std::to_string(count);
    return count > 1 ? "L1   " + number + "   R1" : number;
}

} // namespace expanded_quick_select
