#include "expanded_quick_select/slot_store.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using expanded_quick_select::SlotStore;
using expanded_quick_select::Slots;
namespace fs = std::filesystem;

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path dir = fs::temp_directory_path() / ("eqs-store-test-" + std::to_string(suffix));
    fs::create_directories(dir);
    const Slots page_a{{1, 0, 3, 5, 7, 9, 11, 13}};
    const Slots page_b{{0, 2, 4, 6, 8, 10, 12, 14}};
    SlotStore store(dir / "mod-data");

    assert(!store.load(0).has_value());
    assert(!store.load(-1).has_value());
    assert(!store.save(-1, page_a));
    assert(!store.save(9999, page_a));

    assert(store.save(0, page_a));
    assert(store.save(1, page_b));
    assert(store.load(0).value() == page_a);
    assert(store.load(1).value() == page_b);

    // Updating one save does not leak into another save or leave tmp files.
    assert(store.save(0, page_b));
    assert(store.load(0).value() == page_b);
    assert(store.load(1).value() == page_b);
    for (const auto& entry : fs::directory_iterator(dir / "mod-data")) {
        assert(entry.path().extension() == ".dat");
    }

    // A truncated/corrupt file is rejected, never interpreted as slots.
    const auto file = dir / "mod-data" / "save-slot-0.dat";
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "EQS1";
    }
    assert(!store.load(0).has_value());
    assert(store.save(0, page_a));
    {
        std::fstream bytes(file, std::ios::binary | std::ios::in | std::ios::out);
        bytes.seekp(9);
        bytes.put(static_cast<char>(0x7f));
    }
    assert(!store.load(0).has_value());
    assert(store.load(1).value() == page_b);

    // Invalid item IDs and an unwritable target cannot replace good data.
    const Slots invalid{{1, 2, 3, -1, 5, 6, 7, 8}};
    assert(!store.save(1, invalid));
    assert(store.load(1).value() == page_b);
    const Slots too_high{{1, 2, 3, 40, 5, 6, 7, 8}};
    assert(!store.save(1, too_high));
    const fs::path blocker = dir / "a-file-not-a-directory";
    { std::ofstream out(blocker); out << "x"; }
    SlotStore blocked(blocker);
    assert(!blocked.save(0, page_a));

    fs::remove_all(dir);
}
