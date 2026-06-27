#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

class PipeGuard {
public:
    void configure(const std::vector<std::string>& blocked_pipes,
                   const std::vector<std::string>& blocked_mutexes);
    void scan();

private:
    void report_once(const std::string& key, const std::string& message, const std::string& detail);

    std::vector<std::string> blocked_pipes_;
    std::vector<std::string> blocked_mutexes_;
    std::unordered_set<std::string> reported_;
};

} // namespace anticheat
