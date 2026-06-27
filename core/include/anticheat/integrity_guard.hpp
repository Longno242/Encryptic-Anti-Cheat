#pragma once

#include <anticheat/config.hpp>
#include <string>
#include <vector>

namespace anticheat {

class IntegrityGuard {
public:
    void configure(const IntegrityConfig& config);
    void prime();
    std::vector<std::string> scan();

private:
    IntegrityConfig cfg_;
    std::uint32_t self_crc_{0};
    bool primed_{false};
};

} // namespace anticheat
