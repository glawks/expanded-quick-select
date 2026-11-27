#pragma once

namespace expanded_quick_select::rac2_save_slot {
constexpr bool valid(int slot) noexcept { return slot >= 0 && slot < 4; }
// Each of the two native card records contains four save-file entries.
// Keep the second card distinct if the host ever exposes it.
constexpr int native_key(int card, int slot) noexcept {
    return card >= 0 && card < 2 && valid(slot) ? card * 4 + slot : -1;
}
constexpr int choose(int host, int manual, int native) noexcept {
    if (valid(host)) return host;
    if (manual >= 100 && manual <= 103) return manual;
    return native >= 0 && native < 8 ? native : -1;
}
} // namespace expanded_quick_select::rac2_save_slot
