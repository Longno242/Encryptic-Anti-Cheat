#pragma once

#include <anticheat/config.hpp>
#include <vector>

namespace anticheat {

class OverlayGuard {
public:
    void configure(const std::vector<std::string>& whitelist);
    void scan();

private:
    std::vector<std::string> whitelist_;
};

} // namespace anticheat
