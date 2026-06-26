#pragma once

#include <anticheat/config.hpp>
#include <anticheat/types.hpp>

namespace anticheat {

class Telemetry {
public:
    void configure(const TelemetryConfig& config);
    void queue_violation(const Violation& violation);
    void tick();
    std::string module_hash() const;

private:
    TelemetryConfig cfg_;
    std::uint32_t pending_{0};
};

} // namespace anticheat
