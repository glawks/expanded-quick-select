#pragma once
#include <cstdint>

namespace expanded_quick_select::rac2_close {
// The native paused wheel may return to state 0 before its HUD selector
// finishes. Only use this predicate inside an already observed session,
// with the same load generation and a bounded timeout.
constexpr bool context(bool menu_phase, bool playing_phase,
                       std::int32_t state, std::int32_t arg) noexcept {
    return (menu_phase && state == 4 && (arg == 7 || arg == 0)) ||
           ((playing_phase || menu_phase) && state == 0 && arg == 0);
}
} // namespace expanded_quick_select::rac2_close
