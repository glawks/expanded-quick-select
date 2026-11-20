#pragma once
#include "expanded_quick_select/banks.hpp"
#include <cstddef>

namespace expanded_quick_select {

// Observational only: never refuse assignments based on this statistic.
// Hooks from other mods may legitimately rewrite all eight native slots.
inline std::size_t changed_native_slots(const Slots& before,
                                         const Slots& after) noexcept {
    std::size_t changed = 0;
    for (std::size_t i = 0; i < before.size(); ++i)
        if (before[i] != after[i]) ++changed;
    return changed;
}
} // namespace expanded_quick_select
