#include "expanded_quick_select/slot_store.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace expanded_quick_select {
namespace {
namespace fs = std::filesystem;

constexpr std::size_t kItemCount = 8;
constexpr std::size_t kBodySize = 4 + 2 + 2 + 4 * kItemCount;
constexpr std::size_t kFileSize = kBodySize + 4;
constexpr int kMaxSaveSlot = 127;
// RAC1's quick-select item table is indexed by actual item IDs 0..39.
constexpr std::int32_t kMaxItemId = 39;
using Bytes = std::array<std::uint8_t, kFileSize>;

bool valid_slot(int slot) { return slot >= 0 && slot <= kMaxSaveSlot; }
bool valid_items(const Slots& items) {
    for (const std::int32_t id : items) {
        if (id < 0 || id > kMaxItemId) return false;
    }
    return true;
}

void put_u32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

std::uint32_t get_u32(const Bytes& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

std::uint32_t checksum(const Bytes& bytes) {
    std::uint32_t hash = 2166136261u; // FNV-1a (corruption check, not security)
    for (std::size_t i = 0; i < kBodySize; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

Bytes encode(const Slots& slots) {
    Bytes bytes{};
    bytes[0] = 'E'; bytes[1] = 'Q'; bytes[2] = 'S'; bytes[3] = '1';
    bytes[4] = 1; // format version, little-endian u16
    for (std::size_t i = 0; i < kItemCount; ++i) {
        put_u32(bytes, 8 + 4 * i, static_cast<std::uint32_t>(slots[i]));
    }
    put_u32(bytes, kBodySize, checksum(bytes));
    return bytes;
}

std::optional<Slots> decode(const Bytes& bytes) {
    if (bytes[0] != 'E' || bytes[1] != 'Q' || bytes[2] != 'S' || bytes[3] != '1'
        || bytes[4] != 1 || bytes[5] != 0 || bytes[6] != 0 || bytes[7] != 0
        || get_u32(bytes, kBodySize) != checksum(bytes)) {
        return std::nullopt;
    }
    Slots slots{};
    for (std::size_t i = 0; i < kItemCount; ++i) {
        const std::uint32_t item = get_u32(bytes, 8 + i * 4);
        if (item > static_cast<std::uint32_t>(kMaxItemId)) return std::nullopt;
        slots[i] = static_cast<std::int32_t>(item);
    }
    return slots;
}

bool replace_file(const fs::path& source, const fs::path& destination) {
#ifdef _WIN32
    return ::MoveFileExW(source.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    fs::rename(source, destination, error);
    return !error;
#endif
}
} // namespace

SlotStore::SlotStore(fs::path mod_data_dir)
    : mod_data_dir_(std::move(mod_data_dir)) {}

fs::path SlotStore::path_for(int save_slot) const {
    return mod_data_dir_ / ("save-slot-" + std::to_string(save_slot) + ".dat");
}

std::optional<Slots> SlotStore::load(int save_slot) const {
    if (!valid_slot(save_slot) || mod_data_dir_.empty()) return std::nullopt;
    std::ifstream stream(path_for(save_slot), std::ios::binary);
    if (!stream) return std::nullopt;
    Bytes bytes{};
    stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())
        || stream.peek() != std::char_traits<char>::eof()) {
        return std::nullopt;
    }
    return decode(bytes);
}

bool SlotStore::save(int save_slot, const Slots& secondary) const {
    if (!valid_slot(save_slot) || !valid_items(secondary) || mod_data_dir_.empty()) {
        return false;
    }
    std::error_code error;
    fs::create_directories(mod_data_dir_, error);
    if (error) return false;

    static std::atomic<std::uint64_t> serial{0};
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temporary = path_for(save_slot).string() + "." +
        std::to_string(unique) + "." + std::to_string(serial++) + ".tmp";
    const Bytes bytes = encode(secondary);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        stream.flush();
        if (!stream) {
            stream.close();
            fs::remove(temporary, error);
            return false;
        }
        stream.close();
        if (!stream) {
            fs::remove(temporary, error);
            return false;
        }
    }
    if (!replace_file(temporary, path_for(save_slot))) {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

} // namespace expanded_quick_select
