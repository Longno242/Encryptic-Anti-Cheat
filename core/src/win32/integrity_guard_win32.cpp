#include <anticheat/integrity_guard.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

std::uint32_t crc32(const void* data, std::size_t size) {
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

std::uint32_t self_text_crc() {
    HMODULE mod = GetModuleHandleA("encryptic_core.dll");
    if (!mod) {
        mod = GetModuleHandleA(nullptr);
    }
    if (!mod) {
        return 0;
    }

    const auto* base = reinterpret_cast<const std::uint8_t*>(mod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (std::memcmp(section[i].Name, ".text", 5) == 0) {
            return crc32(base + section[i].VirtualAddress, section[i].Misc.VirtualSize);
        }
    }
    return 0;
}

} // namespace

namespace anticheat {

void IntegrityGuard::configure(const IntegrityConfig& config) {
    cfg_ = config;
    primed_ = false;
}

void IntegrityGuard::prime() {
    if (cfg_.self_integrity) {
        self_crc_ = self_text_crc();
        primed_ = self_crc_ != 0;
    }
}

std::vector<std::string> IntegrityGuard::scan() {
    std::vector<std::string> hits;

    if (cfg_.self_integrity && primed_) {
        const std::uint32_t current = self_text_crc();
        if (current != self_crc_) {
            hits.push_back("encryptic_core .text CRC mismatch");
        }
    }

    if (cfg_.game_file_integrity) {
        for (const auto& entry : cfg_.file_manifest) {
            const std::string hash = anticheat::platform::sha256_file(entry.path);
            if (hash.empty()) {
                hits.push_back("missing file: " + entry.path);
            } else if (hash != entry.sha256_hex) {
                hits.push_back("hash mismatch: " + entry.path);
            }
        }
    }

    return hits;
}

} // namespace anticheat

#else

namespace anticheat {

void IntegrityGuard::configure(const IntegrityConfig&) {}
void IntegrityGuard::prime() {}
std::vector<std::string> IntegrityGuard::scan() { return {}; }

} // namespace anticheat

#endif
