#pragma once

#include <string>
#include <vector>

namespace anticheat {

class ProcessGuard {
public:
    void configure(const std::vector<std::string>& blocked_names);
    void scan();

private:
    std::vector<std::string> blocked_;
};

} // namespace anticheat
