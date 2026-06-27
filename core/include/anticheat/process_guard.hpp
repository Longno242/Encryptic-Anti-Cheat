#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

class ProcessGuard {
public:
    void configure(const std::vector<std::string>& blocked_names);
    void scan();

private:
    std::vector<std::string> blocked_;
    std::unordered_set<std::string> reported_;
};

} // namespace anticheat
