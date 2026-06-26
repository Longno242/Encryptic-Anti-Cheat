#pragma once

#include <anticheat/config.hpp>
#include <anticheat/types.hpp>

namespace anticheat {

class ScreenshotGuard {
public:
    void configure(const ScreenshotConfig& config);
    void capture_on_violation(const Violation& violation);

private:
    ScreenshotConfig cfg_;
};

} // namespace anticheat
