#pragma once
#include "expanded_quick_select/banks.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace expanded_quick_select::rac2_inventory {
inline constexpr std::int32_t kMaxItemId = 55;
using Order = std::array<std::uint8_t, 32>;
using Owned = std::array<std::uint8_t, kMaxItemId + 1>;
using Changed = std::array<bool, kMaxPages>;

inline bool is_owned(const Owned& owned, std::int32_t item) noexcept {
    return item > 0 && item <= kMaxItemId && owned[static_cast<std::size_t>(item)] != 0;
}

// Native order contains weapons/gadgets, with a flag in bit 7.
// Never change the primary page or rearrange existing sidecar slots.
inline Changed fill_owned_pages(Banks& banks, const Order& order,
                                const Owned& owned) noexcept {
    Changed changed{};
    std::array<bool, kMaxItemId + 1> listed{};
    for (std::size_t p = 0; p < banks.page_count(); ++p) {
        for (const auto item : banks.slots(page_at(p))) {
            if (item > 0 && item <= kMaxItemId) listed[static_cast<std::size_t>(item)] = true;
        }
    }
    for (const auto entry : order) {
        if (entry == 0xFF) continue;
        const auto item = static_cast<std::int32_t>(entry & 0x7F);
        if (!is_owned(owned, item) || listed[static_cast<std::size_t>(item)]) continue;
        for (std::size_t p = 1; p < banks.page_count(); ++p) {
            const auto& slots = banks.slots(page_at(p));
            const auto empty = std::find(slots.begin(), slots.end(), 0);
            if (empty == slots.end()) continue;
            if (banks.set_slot(page_at(p), static_cast<std::size_t>(empty - slots.begin()), item)) {
                changed[p] = true;
                listed[static_cast<std::size_t>(item)] = true;
            }
            break;
        }
    }
    return changed;
}
} // namespace expanded_quick_select::rac2_inventory
