#include "expanded_quick_select/identity.hpp"
#include "openrac/mod.hpp"

static void load(openrac::Mod& mod) {
    if (mod.game() != "rac1") {
        throw openrac::Error("Expanded Quick Select supports RAC1 only");
    }
    mod.info("Expanded Quick Select: scaffold loaded; wheel hooks not installed");
}

OPENRAC_MOD_CPP(expanded_quick_select::kModId,
                expanded_quick_select::kModVersion, load)
