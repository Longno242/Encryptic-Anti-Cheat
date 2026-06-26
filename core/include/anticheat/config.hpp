#pragma once

#include <anticheat/types.hpp>
#include <anticheat/watermark.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace anticheat {

struct TelemetryConfig {
    bool enabled{false};
    std::string endpoint_url;
    std::string session_id;
    std::string api_key;
    std::uint32_t flush_interval_ms{5000};
};

struct HeartbeatConfig {
    bool enabled{false};
    std::string endpoint_url;
    std::string session_token;
    std::string hmac_secret;
    std::uint32_t interval_ms{30000};
    std::uint32_t miss_threshold{2};
};

struct IntegrityConfig {
    bool self_integrity{true};
    bool game_file_integrity{false};
    std::vector<FileHashEntry> file_manifest;
};

struct ScreenshotConfig {
    bool on_critical{false};
    std::string output_dir{"encryptic_captures"};
};

struct Config {
    ProtectionPreset preset{ProtectionPreset::Balanced};

    bool dll_guard{true};
    bool debugger_guard{true};
    bool hook_guard{true};
    bool memory_guard{true};
    bool module_whitelist{false};
    bool thread_guard{true};
    bool timing_guard{true};
    bool handle_guard{true};
    bool process_guard{true};
    bool vm_guard{false};
    bool overlay_guard{true};
    bool input_guard{true};
    bool watchdog_guard{true};
    bool anti_tamper{true};

    bool block_unknown_dlls{false};
    bool terminate_on_debugger{false};
    bool require_signature{false};
    bool kick_on_critical{false};
    bool log_violations{true};

    std::uint32_t hook_scan_interval_ms{5000};
    std::uint32_t memory_scan_interval_ms{3000};
    std::uint32_t thread_scan_interval_ms{4000};
    std::uint32_t handle_scan_interval_ms{10000};
    std::uint32_t timing_check_interval_ms{1000};
    std::uint32_t process_scan_interval_ms{8000};
    std::uint32_t overlay_scan_interval_ms{6000};
    std::uint32_t input_scan_interval_ms{2000};
    std::uint32_t watchdog_interval_ms{2000};
    std::uint32_t integrity_scan_interval_ms{15000};
    double max_timing_drift_ms{120.0};

    std::vector<std::string> allowed_modules;
    std::vector<std::string> blocked_processes;
    std::vector<std::string> debugger_blocked_services;
    std::vector<std::string> overlay_whitelist;

    WatermarkConfig watermark;
    TelemetryConfig telemetry;
    HeartbeatConfig heartbeat;
    IntegrityConfig integrity;
    ScreenshotConfig screenshot;

    ViolationCallback on_violation;

    static Config from_preset(ProtectionPreset preset);
    void apply_preset(ProtectionPreset preset);
};

bool load_config_from_file(const std::string& path, Config& out);
bool load_config_from_json(const std::string& json, Config& out);

} // namespace anticheat
