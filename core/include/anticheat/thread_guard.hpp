#pragma once

namespace anticheat {

class ThreadGuard {
public:
    void establish_baseline();
    void scan();

private:
    unsigned baseline_count_{0};
};

} // namespace anticheat
