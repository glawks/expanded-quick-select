#include "expanded_quick_select/identity.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string_view>

int main() {
    using namespace expanded_quick_select;
    assert(std::string_view(kModId) == "expanded-quick-select");
    assert(std::string_view(kModVersion) == "0.1.6");
}
