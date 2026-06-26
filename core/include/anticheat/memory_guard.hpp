#pragma once

#include <anticheat/types.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace anticheat {

class MemoryGuard {
public:
    void register_region(const MemoryRegion& region);
    void scan();

private:
    struct RegionState {
        MemoryRegion region;
        std::uint32_t checksum{};
    };

    std::uint32_t crc32(const void* data, std::size_t size) const;
    std::vector<RegionState> regions_;
};

} // namespace anticheat
