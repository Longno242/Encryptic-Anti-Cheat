#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

class ThreatIntelGuard {
public:
    void configure(const std::vector<std::string>& blocked_modules,
                   const std::vector<std::string>& blocked_window_classes,
                   const std::vector<std::string>& blocked_window_titles,
                   const std::vector<std::string>& blocked_path_segments,
                   const std::vector<std::string>& blocked_drivers);
    void scan();

private:
    void report_once(const std::string& key, const std::string& message, const std::string& detail);

    std::vector<std::string> blocked_modules_;
    std::vector<std::string> blocked_window_classes_;
    std::vector<std::string> blocked_window_titles_;
    std::vector<std::string> blocked_path_segments_;
    std::vector<std::string> blocked_drivers_;
    std::unordered_set<std::string> reported_;
};

} // namespace anticheat
