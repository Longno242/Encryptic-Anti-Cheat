#pragma once

namespace anticheat {

class InputGuard {
public:
    void prime();
    bool scan();

private:
    int consecutive_hits_{0};
};

} // namespace anticheat
