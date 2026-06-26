#include <anticheat/integrity_guard.hpp>

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

std::string sha256_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }

    std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty()) {
        return {};
    }

    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return {};
    }
    if (!CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        return {};
    }
    if (!CryptHashData(hash, reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size()), 0)) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        return {};
    }

    BYTE digest[32]{};
    DWORD len = 32;
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &len, 0)) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        return {};
    }

    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);

    std::ostringstream oss;
    for (DWORD i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return oss.str();
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
            const std::string hash = sha256_file(entry.path);
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
