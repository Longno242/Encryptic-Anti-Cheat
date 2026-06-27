#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

class InjectionGuard {
public:
    void configure(const std::vector<std::string>& blocked_processes);
    void scan();

private:
    void report_once(const std::string& key, const std::string& detail);

    std::vector<std::string> blocked_;
    std::unordered_set<std::string> reported_;
};

} // namespace anticheat
