#include <anticheat/memory_guard.hpp>
#include <anticheat/encryptic.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>

namespace anticheat {

std::uint32_t MemoryGuard::crc32(const void* data, std::size_t size) const {
    static constexpr std::uint32_t polynomial = 0xEDB88320u;
    std::uint32_t crc = 0xFFFFFFFFu;
    const auto* bytes = static_cast<const std::uint8_t*>(data);

    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (polynomial & mask);
        }
    }

    return ~crc;
}

void MemoryGuard::register_region(const MemoryRegion& region) {
    if (!region.address || region.size == 0) {
        return;
    }

    RegionState state;
    state.region = region;
    state.checksum = crc32(region.address, region.size);
    regions_.push_back(state);
}

void MemoryGuard::scan() {
    for (const auto& state : regions_) {
        const auto current = crc32(state.region.address, state.region.size);
        if (current != state.checksum) {
            Encryptic::instance().report_violation({
                ViolationType::MemoryTampered,
                ViolationSeverity::Critical,
                "Memory region tampered",
                state.region.tag.empty() ? "unnamed" : state.region.tag,
            });
        }

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(state.region.address, &mbi, sizeof(mbi))) {
            if (mbi.Protect == PAGE_EXECUTE_READWRITE || mbi.Protect == PAGE_EXECUTE_WRITECOPY) {
                Encryptic::instance().report_violation({
                    ViolationType::MemoryTampered,
                    ViolationSeverity::High,
                    "Executable memory is writable",
                    state.region.tag,
                });
            }
        }
    }
}

} // namespace anticheat

#else

namespace anticheat {

std::uint32_t MemoryGuard::crc32(const void* data, std::size_t size) const {
    (void)data;
    (void)size;
    return 0;
}

void MemoryGuard::register_region(const MemoryRegion&) {}
void MemoryGuard::scan() {}

} // namespace anticheat

#endif
