#pragma once

#include <anticheat/types.hpp>
#include <encryptic_kernel.h>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

struct KernelGuardSettings {
    bool enabled{false};
    bool protect_handles{true};
    bool block_drivers{true};
    bool block_modules{true};
    bool block_global_cheats{false};
    std::uint32_t poll_interval_ms{1000};
    std::string device_path{"\\\\.\\EncrypticGuard"};
};

class KernelGuard {
public:
    void configure(const KernelGuardSettings& settings,
                     const std::vector<std::string>& blocked_processes,
                     const std::vector<std::string>& blocked_drivers,
                     const std::vector<std::string>& blocked_modules);
    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }
    void scan();

private:
    bool push_config();
    void handle_event(const ENCRYPTIC_KERNEL_EVENT& ev);
    void report_once(const std::string& key, ViolationType type, const std::string& message,
                     const std::string& detail);

    std::uint64_t session_token_{0};

    KernelGuardSettings settings_;
    std::vector<std::string> blocked_processes_;
    std::vector<std::string> blocked_drivers_;
    std::vector<std::string> blocked_modules_;
    void* device_handle_{nullptr};
    bool connected_{false};
    bool warned_no_driver_{false};
    std::unordered_set<std::string> reported_;
};

} // namespace anticheat
