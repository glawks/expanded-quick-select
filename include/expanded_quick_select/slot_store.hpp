#pragma once

#include "expanded_quick_select/banks.hpp"

#include <filesystem>
#include <optional>

namespace expanded_quick_select {

// Persists pages 2..8 per RAC1 save slot. The v1 page-2 file format and name
// are intentionally unchanged, so upgrading from v0.1.6 cannot lose it.
class SlotStore {
public:
    explicit SlotStore(std::filesystem::path mod_data_dir);

    std::optional<Slots> load(int save_slot) const;
    bool save(int save_slot, const Slots& secondary) const;
    std::optional<Slots> load(int save_slot, Page page) const;
    bool save(int save_slot, Page page, const Slots& items) const;

private:
    std::filesystem::path path_for(int save_slot, Page page) const;
    std::filesystem::path mod_data_dir_;
};

} // namespace expanded_quick_select
