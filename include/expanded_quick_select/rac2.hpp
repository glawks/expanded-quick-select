#pragma once
namespace openrac { class Mod; }
namespace expanded_quick_select {
// The game-specific implementation is opt-in and isolated from RAC1's hooks.
void load_rac2(openrac::Mod& mod);
}
