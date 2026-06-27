#pragma once

#include <anticheat/types.hpp>
#include <string>
#include <vector>

namespace anticheat {

class DebuggerGuard {
public:
    void configure(const std::vector<std::string>& blocked_services,
                   const std::vector<std::string>& blocked_processes);
    std::vector<std::string> scan(bool terminate_on_detect);

private:
    std::vector<std::string> blocked_services_;
    std::vector<std::string> blocked_processes_;
};

} // namespace anticheat
