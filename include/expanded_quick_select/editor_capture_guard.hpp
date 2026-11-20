#pragma once

#include "expanded_quick_select/sessions.hpp"
#include <cstddef>

namespace expanded_quick_select {

/* Secondary pages are owned by EQS. A single native Cross assignment can
   update its target and clear the old location of that same weapon (2 cells).
   A whole-eight-slot rewrite by another plugin must never be adopted as a
   legitimate edit. Without Cross, any changed slots are unexpected.
   Do not apply this guard to page one: the native game owns that page. */
struct EditorCaptureResult {
    Slots slots;
    bool rejected;
};

inline EditorCaptureResult guard_secondary_capture(
    const Slots& expected, const Slots& observed, bool cross_pressed) noexcept {
    if (!valid_game_slots(observed)) return {expected, true};
    std::size_t changes = 0;
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (expected[i] != observed[i]) ++changes;
    if (changes != 0 && (!cross_pressed || changes > 2))
        return {expected, true};
    return {observed, false};
}
} // namespace expanded_quick_select
