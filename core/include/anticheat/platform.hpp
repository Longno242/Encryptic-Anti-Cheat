#pragma once

#include <string>

namespace anticheat::platform {

std::string basename(const std::string& path);
std::string to_lower(std::string s);
bool verify_authenticode(const std::string& path);

} // namespace anticheat::platform
