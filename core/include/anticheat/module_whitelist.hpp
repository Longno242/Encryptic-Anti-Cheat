#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

class ModuleWhitelist {
public:
    void configure(const std::vector<std::string>& allowed, bool require_signature);
    void establish_baseline();
    void scan();

private:
    std::unordered_set<std::string> allowed_;
    std::unordered_set<std::string> baseline_;
    bool require_signature_{false};
};

} // namespace anticheat
