#pragma once

#include <anticheat/config.hpp>

namespace anticheat {

class HeartbeatGuard {
public:
    void configure(const HeartbeatConfig& config);
    void tick();
    bool missed_threshold() const;

private:
    HeartbeatConfig cfg_;
    std::uint32_t misses_{0};
    std::uint64_t last_send_ms_{0};
};

} // namespace anticheat
