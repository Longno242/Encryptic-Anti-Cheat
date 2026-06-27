#pragma once

#include <anticheat/config.hpp>
#include <anticheat/types.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

namespace anticheat {

class DllGuard;
class DebuggerGuard;
class HookGuard;
class MemoryGuard;
class ModuleWhitelist;
class ThreadGuard;
class TimingGuard;
class HandleGuard;
class ProcessGuard;
class ThreatIntelGuard;
class InjectionGuard;
class AntiBypassGuard;
class HashGuard;
class PipeGuard;
class KernelGuard;
class VmGuard;
class OverlayGuard;
class InputGuard;
class Telemetry;
class HeartbeatGuard;
class IntegrityGuard;
class ScreenshotGuard;
class WatchdogGuard;

class Encryptic {
public:
    static Encryptic& instance();

    void start(const Config& config);
    void start_from_file(const std::string& path);
    void stop();
    bool is_running() const;

    void tick();
    void register_memory_region(const MemoryRegion& region);
    void report_violation(Violation violation);
    void show_watermark();

    const Config& config() const { return config_; }

private:
    Encryptic() = default;
    void run_scheduled_guards(std::chrono::steady_clock::time_point now);
    void ensure_session_id();
    void run_anti_tamper_checks();

    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> tick_calls_{0};

    std::unique_ptr<DllGuard> dll_guard_;
    std::unique_ptr<DebuggerGuard> debugger_guard_;
    std::unique_ptr<HookGuard> hook_guard_;
    std::unique_ptr<MemoryGuard> memory_guard_;
    std::unique_ptr<ModuleWhitelist> module_whitelist_;
    std::unique_ptr<ThreadGuard> thread_guard_;
    std::unique_ptr<TimingGuard> timing_guard_;
    std::unique_ptr<HandleGuard> handle_guard_;
    std::unique_ptr<ProcessGuard> process_guard_;
    std::unique_ptr<ThreatIntelGuard> threat_intel_guard_;
    std::unique_ptr<InjectionGuard> injection_guard_;
    std::unique_ptr<AntiBypassGuard> anti_bypass_guard_;
    std::unique_ptr<HashGuard> hash_guard_;
    std::unique_ptr<PipeGuard> pipe_guard_;
    std::unique_ptr<KernelGuard> kernel_guard_;
    std::unique_ptr<VmGuard> vm_guard_;
    std::unique_ptr<OverlayGuard> overlay_guard_;
    std::unique_ptr<InputGuard> input_guard_;
    std::unique_ptr<Telemetry> telemetry_;
    std::unique_ptr<HeartbeatGuard> heartbeat_;
    std::unique_ptr<IntegrityGuard> integrity_;
    std::unique_ptr<ScreenshotGuard> screenshot_;
    std::unique_ptr<WatchdogGuard> watchdog_;

    std::chrono::steady_clock::time_point last_hook_scan_{};
    std::chrono::steady_clock::time_point last_memory_scan_{};
    std::chrono::steady_clock::time_point last_thread_scan_{};
    std::chrono::steady_clock::time_point last_handle_scan_{};
    std::chrono::steady_clock::time_point last_timing_scan_{};
    std::chrono::steady_clock::time_point last_process_scan_{};
    std::chrono::steady_clock::time_point last_threat_intel_scan_{};
    std::chrono::steady_clock::time_point last_injection_scan_{};
    std::chrono::steady_clock::time_point last_anti_bypass_scan_{};
    std::chrono::steady_clock::time_point last_hash_scan_{};
    std::chrono::steady_clock::time_point last_pipe_scan_{};
    std::chrono::steady_clock::time_point last_kernel_scan_{};
    std::chrono::steady_clock::time_point last_overlay_scan_{};
    std::chrono::steady_clock::time_point last_input_scan_{};
    std::chrono::steady_clock::time_point last_integrity_scan_{};

    std::mutex mutex_;
};

} // namespace anticheat
