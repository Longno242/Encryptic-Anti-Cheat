#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#pragma comment(lib, "wintrust.lib")

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace anticheat::platform {
namespace {

std::string digest_to_hex(const BYTE* digest, DWORD len) {
    std::ostringstream oss;
    for (DWORD i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return oss.str();
}

bool sha256_hash(const BYTE* data, DWORD size, BYTE out[32]) {
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return false;
    }
    if (!CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        return false;
    }
    if (!CryptHashData(hash, data, size, 0)) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        return false;
    }
    DWORD len = 32;
    const BOOL ok = CryptGetHashParam(hash, HP_HASHVAL, out, &len, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    return ok != FALSE;
}

} // namespace

bool verify_authenticode(const std::string& path) {
    std::wstring wide(path.begin(), path.end());

    WINTRUST_FILE_INFO file_info{};
    file_info.cbStruct = sizeof(file_info);
    file_info.pcwszFilePath = wide.c_str();

    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA trust_data{};
    trust_data.cbStruct = sizeof(trust_data);
    trust_data.dwUIChoice = WTD_UI_NONE;
    trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
    trust_data.dwUnionChoice = WTD_CHOICE_FILE;
    trust_data.pFile = &file_info;
    trust_data.dwStateAction = WTD_STATEACTION_VERIFY;

    const LONG status = WinVerifyTrust(nullptr, &policy, &trust_data);
    trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policy, &trust_data);

    return status == ERROR_SUCCESS;
}

std::string sha256_bytes(const void* data, std::size_t size) {
    if (!data || size == 0) {
        return {};
    }
    BYTE digest[32]{};
    if (!sha256_hash(static_cast<const BYTE*>(data), static_cast<DWORD>(size), digest)) {
        return {};
    }
    return digest_to_hex(digest, 32);
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
    return sha256_bytes(data.data(), data.size());
}

std::string hmac_sha256_hex(const std::string& key, const std::string& data) {
    constexpr std::size_t block_size = 64;
    std::string k = key;
    if (k.size() > block_size) {
        k = sha256_bytes(k.data(), k.size());
    }
    if (k.size() < block_size) {
        k.append(block_size - k.size(), '\0');
    }

    std::string ipad(block_size, '\x36');
    std::string opad(block_size, '\x5c');
    for (std::size_t i = 0; i < block_size; ++i) {
        ipad[i] ^= k[i];
        opad[i] ^= k[i];
    }

    std::string inner = ipad + data;
    const std::string inner_hash = sha256_bytes(inner.data(), inner.size());
    if (inner_hash.size() != 64) {
        return {};
    }

    std::vector<BYTE> inner_bytes(32);
    for (std::size_t i = 0; i < 32; ++i) {
        inner_bytes[i] = static_cast<BYTE>(std::stoul(inner_hash.substr(i * 2, 2), nullptr, 16));
    }

    std::string outer = opad + std::string(reinterpret_cast<char*>(inner_bytes.data()), inner_bytes.size());
    return sha256_bytes(outer.data(), outer.size());
}

} // namespace anticheat::platform

#else

namespace anticheat::platform {

bool verify_authenticode(const std::string&) {
    return false;
}

std::string sha256_file(const std::string&) { return {}; }
std::string sha256_bytes(const void*, std::size_t) { return {}; }
std::string hmac_sha256_hex(const std::string&, const std::string&) { return {}; }

} // namespace anticheat::platform

#endif
