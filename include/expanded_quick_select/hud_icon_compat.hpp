#pragma once
#include <cstdint>

namespace expanded_quick_select {

// Revolverator projects its logical weapon onto the native Walloper carrier
// (item 18). The on-screen quick-select cache can retain Walloper's icon
// while the live item definition changes, or vice versa. The item ID itself
// must still match. Restrict tolerance to this exact, known icon pair: a
// mismatch for any other item remains a layout validation failure.
constexpr bool hud_icon_compatible(std::int32_t item, std::int32_t cached_icon,
                                   std::uint16_t live_icon) noexcept {
    if (item == 0 || cached_icon == live_icon) return true;
    constexpr std::int32_t kNativeWalloperItem = 18;
    constexpr std::int32_t kNativeWalloperIcon = 60018;
    constexpr std::int32_t kRevolveratorIcon = 60037;
    return item == kNativeWalloperItem &&
           ((cached_icon == kNativeWalloperIcon && live_icon == kRevolveratorIcon) ||
            (cached_icon == kRevolveratorIcon && live_icon == kNativeWalloperIcon));
}

} // namespace expanded_quick_select
