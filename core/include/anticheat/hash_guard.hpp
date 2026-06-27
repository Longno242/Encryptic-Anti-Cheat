#pragma once

#include <anticheat/types.hpp>
#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

class HashGuard {
public:
    void configure(const std::vector<CheatHashEntry>& known_hashes);
    void scan();

private:
    void report_once(const std::string& key, const std::string& detail);

    std::vector<CheatHashEntry> known_;
    std::unordered_set<std::string> reported_;
};

} // namespace anticheat
