#pragma once

#include <atomic>
#include <cstdint>

namespace anticheat {

class WatchdogGuard {
public:
    void start(std::uint32_t interval_ms);
    void stop();
    void ping();
    bool scan();

private:
    std::atomic<std::uint64_t> tick_count_{0};
    std::atomic<std::uint64_t> last_ping_ms_{0};
    std::atomic<bool> running_{false};
    std::uint32_t interval_ms_{2000};
    std::atomic<std::uint32_t> canary_{0xEAC12345u};
};

} // namespace anticheat
