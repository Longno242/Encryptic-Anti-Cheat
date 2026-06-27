#pragma once

#include <string>
#include <unordered_set>

namespace anticheat {

class AntiBypassGuard {
public:
    void scan();

private:
    void report_once(const std::string& key, const std::string& detail);

    std::unordered_set<std::string> reported_;
};

} // namespace anticheat
