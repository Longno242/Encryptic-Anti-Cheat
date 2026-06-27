#pragma once

#include <string>

namespace anticheat::platform {

std::string basename(const std::string& path);
std::string to_lower(std::string s);
bool verify_authenticode(const std::string& path);
std::string sha256_file(const std::string& path);
std::string sha256_bytes(const void* data, std::size_t size);
std::string hmac_sha256_hex(const std::string& key, const std::string& data);

} // namespace anticheat::platform
