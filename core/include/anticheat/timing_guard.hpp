#pragma once

namespace anticheat {

class TimingGuard {
public:
    void reset();
    bool scan(double max_drift_ms);

private:
    long long qpc_base_{0};
    unsigned long long tick_base_{0};
    long long qpc_freq_{0};
    bool primed_{false};
};

} // namespace anticheat
