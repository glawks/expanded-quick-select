#pragma once

#include "expanded_quick_select/banks.hpp"

#include <filesystem>
#include <optional>

namespace expanded_quick_select {

// Owns only the second Quick Select page; never modifies RAC1 save cards.
class SlotStore {
public:
    explicit SlotStore(std::filesystem::path mod_data_dir);

    std::optional<Slots> load(int save_slot) const;
    bool save(int save_slot, const Slots& secondary) const;

private:
    std::filesystem::path path_for(int save_slot) const;
    std::filesystem::path mod_data_dir_;
};

} // namespace expanded_quick_select
