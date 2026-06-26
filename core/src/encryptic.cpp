#include <anticheat/encryptic.hpp>
#include <anticheat/dll_guard.hpp>
#include <anticheat/debugger_guard.hpp>
#include <anticheat/hook_guard.hpp>
#include <anticheat/memory_guard.hpp>
#include <anticheat/module_whitelist.hpp>
#include <anticheat/thread_guard.hpp>
#include <anticheat/timing_guard.hpp>
#include <anticheat/handle_guard.hpp>
#include <anticheat/process_guard.hpp>
#include <anticheat/vm_guard.hpp>
#include <anticheat/overlay_guard.hpp>
#include <anticheat/input_guard.hpp>
#include <anticheat/telemetry.hpp>
#include <anticheat/heartbeat_guard.hpp>
#include <anticheat/integrity_guard.hpp>
#include <anticheat/screenshot_guard.hpp>
#include <anticheat/watchdog_guard.hpp>
#include <anticheat/watermark.hpp>
#include <anticheat/c_api.h>

#include <chrono>
#include <random>
#include <sstream>

namespace anticheat {

Encryptic& Encryptic::instance() {
    static Encryptic inst;
    return inst;
}

void Encryptic::ensure_session_id() {
    if (!config_.telemetry.session_id.empty() &&
        config_.telemetry.session_id != "auto-generated-at-runtime") {
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    std::ostringstream sid;
    sid << "enc_";
    for (int i = 0; i < 16; ++i) {
        sid << std::hex << dist(gen);
    }
    config_.telemetry.session_id = sid.str();
}

void Encryptic::start_from_file(const std::string& path) {
    Config cfg = Config::from_preset(ProtectionPreset::Balanced);
    if (load_config_from_file(path, cfg)) {
        start(cfg);
    } else {
        start(cfg);
    }
}

void Encryptic::show_watermark() {
    if (config_.watermark.enabled && config_.watermark.show_on_launch) {
        Watermark::show_blocking(config_.watermark);
    }
}

void Encryptic::start(const Config& config) {
    std::lock_guard lock(mutex_);
    if (running_) {
        return;
    }

    config_ = config;
    if (config_.preset != ProtectionPreset::Custom) {
        config_.apply_preset(config_.preset);
    }

    ensure_session_id();

    if (config_.blocked_processes.empty()) {
        config_.blocked_processes = {
            "cheatengine", "x64dbg", "x32dbg", "ida", "ida64", "processhacker",
            "reclass", "httpdebugger", "ollydbg", "windbg", "immunitydebugger",
        };
    }

    if (config_.watermark.enabled && config_.watermark.show_on_launch && config_.watermark.blocking) {
        Watermark::show_blocking(config_.watermark);
    }

    const auto now = std::chrono::steady_clock::now();
    last_hook_scan_ = now;
    last_memory_scan_ = now;
    last_thread_scan_ = now;
    last_handle_scan_ = now;
    last_timing_scan_ = now;
    last_process_scan_ = now;
    last_overlay_scan_ = now;
    last_input_scan_ = now;
    last_integrity_scan_ = now;

    telemetry_ = std::make_unique<Telemetry>();
    telemetry_->configure(config_.telemetry);

    screenshot_ = std::make_unique<ScreenshotGuard>();
    screenshot_->configure(config_.screenshot);

    if (config_.dll_guard) {
        dll_guard_ = std::make_unique<DllGuard>();
        dll_guard_->start(config_.block_unknown_dlls, config_.allowed_modules);
        dll_guard_->install_load_library_hook();
        dll_guard_->scan();
    }

    if (config_.debugger_guard) {
        debugger_guard_ = std::make_unique<DebuggerGuard>();
        debugger_guard_->configure(config_.debugger_blocked_services, config_.blocked_processes);
        for (const auto& hit : debugger_guard_->scan(config_.terminate_on_debugger)) {
            report_violation({
                ViolationType::DebuggerDetected,
                ViolationSeverity::Critical,
                "Debugger detected",
                hit,
            });
        }
    }

    if (config_.hook_guard) {
        hook_guard_ = std::make_unique<HookGuard>();
        hook_guard_->prime();
    }

    if (config_.memory_guard) {
        memory_guard_ = std::make_unique<MemoryGuard>();
    }

    if (config_.module_whitelist) {
        module_whitelist_ = std::make_unique<ModuleWhitelist>();
        module_whitelist_->configure(config_.allowed_modules, config_.require_signature);
        module_whitelist_->establish_baseline();
    }

    if (config_.thread_guard) {
        thread_guard_ = std::make_unique<ThreadGuard>();
        thread_guard_->establish_baseline();
    }

    if (config_.timing_guard) {
        timing_guard_ = std::make_unique<TimingGuard>();
        timing_guard_->reset();
    }

    if (config_.handle_guard) {
        handle_guard_ = std::make_unique<HandleGuard>();
    }

    if (config_.process_guard) {
        process_guard_ = std::make_unique<ProcessGuard>();
        process_guard_->configure(config_.blocked_processes);
    }

    if (config_.vm_guard) {
        vm_guard_ = std::make_unique<VmGuard>();
        if (vm_guard_->scan()) {
            report_violation({
                ViolationType::VirtualMachineDetected,
                ViolationSeverity::Medium,
                "Virtual machine environment detected",
                "VM heuristics triggered at startup",
            });
        }
    }

    if (config_.overlay_guard) {
        overlay_guard_ = std::make_unique<OverlayGuard>();
        overlay_guard_->configure(config_.overlay_whitelist);
    }

    if (config_.input_guard) {
        input_guard_ = std::make_unique<InputGuard>();
        input_guard_->prime();
    }

    if (config_.integrity.self_integrity || config_.integrity.game_file_integrity) {
        integrity_ = std::make_unique<IntegrityGuard>();
        integrity_->configure(config_.integrity);
        integrity_->prime();
        for (const auto& hit : integrity_->scan()) {
            report_violation({
                ViolationType::SelfIntegrityFailed,
                ViolationSeverity::Critical,
                "Integrity check failed",
                hit,
            });
        }
    }

    if (config_.heartbeat.enabled) {
        heartbeat_ = std::make_unique<HeartbeatGuard>();
        heartbeat_->configure(config_.heartbeat);
    }

    if (config_.watchdog_guard) {
        watchdog_ = std::make_unique<WatchdogGuard>();
        watchdog_->start(config_.watchdog_interval_ms);
    }

    running_ = true;
}

void Encryptic::stop() {
    std::lock_guard lock(mutex_);
    if (!running_) {
        return;
    }

    if (watchdog_) {
        watchdog_->stop();
    }

    if (dll_guard_) {
        dll_guard_->remove_load_library_hook();
    }

    dll_guard_.reset();
    debugger_guard_.reset();
    hook_guard_.reset();
    memory_guard_.reset();
    module_whitelist_.reset();
    thread_guard_.reset();
    timing_guard_.reset();
    handle_guard_.reset();
    process_guard_.reset();
    vm_guard_.reset();
    overlay_guard_.reset();
    input_guard_.reset();
    telemetry_.reset();
    heartbeat_.reset();
    integrity_.reset();
    screenshot_.reset();
    watchdog_.reset();
    running_ = false;
}

bool Encryptic::is_running() const {
    return running_;
}

void Encryptic::run_anti_tamper_checks() {
    if (!config_.anti_tamper) {
        return;
    }

    const std::uint64_t ticks = tick_calls_.load();
    if (ticks > 0 && ticks % 97 == 0 && integrity_) {
        for (const auto& hit : integrity_->scan()) {
            report_violation({
                ViolationType::TamperDetected,
                ViolationSeverity::Critical,
                "Anti-tamper integrity failure",
                hit,
            });
        }
    }
}

void Encryptic::tick() {
    if (!running_) {
        return;
    }

    ++tick_calls_;
    if (watchdog_) {
        watchdog_->ping();
    }

    const auto now = std::chrono::steady_clock::now();

    if (debugger_guard_) {
        for (const auto& hit : debugger_guard_->scan(config_.terminate_on_debugger)) {
            report_violation({
                ViolationType::DebuggerDetected,
                ViolationSeverity::Critical,
                "Debugger detected",
                hit,
            });
        }
    }

    if (dll_guard_) {
        dll_guard_->scan();
    }

    if (heartbeat_) {
        heartbeat_->tick();
    }

    if (telemetry_) {
        telemetry_->tick();
    }

    run_scheduled_guards(now);
    run_anti_tamper_checks();
}

void Encryptic::run_scheduled_guards(std::chrono::steady_clock::time_point now) {
    const auto elapsed_ms = [&](const auto& last, std::uint32_t interval) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() >=
               static_cast<long long>(interval);
    };

    if (hook_guard_ && elapsed_ms(last_hook_scan_, config_.hook_scan_interval_ms)) {
        hook_guard_->scan();
        last_hook_scan_ = now;
    }

    if (memory_guard_ && elapsed_ms(last_memory_scan_, config_.memory_scan_interval_ms)) {
        memory_guard_->scan();
        last_memory_scan_ = now;
    }

    if (module_whitelist_ && elapsed_ms(last_memory_scan_, config_.memory_scan_interval_ms)) {
        module_whitelist_->scan();
    }

    if (thread_guard_ && elapsed_ms(last_thread_scan_, config_.thread_scan_interval_ms)) {
        thread_guard_->scan();
        last_thread_scan_ = now;
    }

    if (timing_guard_ && elapsed_ms(last_timing_scan_, config_.timing_check_interval_ms)) {
        if (timing_guard_->scan(config_.max_timing_drift_ms)) {
            report_violation({
                ViolationType::TimingAnomaly,
                ViolationSeverity::High,
                "Timing anomaly detected",
                "Clock drift exceeds configured threshold",
            });
        }
        last_timing_scan_ = now;
    }

    if (handle_guard_ && elapsed_ms(last_handle_scan_, config_.handle_scan_interval_ms)) {
        handle_guard_->scan();
        last_handle_scan_ = now;
    }

    if (process_guard_ && elapsed_ms(last_process_scan_, config_.process_scan_interval_ms)) {
        process_guard_->scan();
        last_process_scan_ = now;
    }

    if (overlay_guard_ && elapsed_ms(last_overlay_scan_, config_.overlay_scan_interval_ms)) {
        overlay_guard_->scan();
        last_overlay_scan_ = now;
    }

    if (input_guard_ && elapsed_ms(last_input_scan_, config_.input_scan_interval_ms)) {
        if (input_guard_->scan()) {
            report_violation({
                ViolationType::SyntheticInputDetected,
                ViolationSeverity::High,
                "Synthetic input detected",
                "GetAsyncKeyState vs GetKeyState mismatch",
            });
        }
        last_input_scan_ = now;
    }

    if (integrity_ && elapsed_ms(last_integrity_scan_, config_.integrity_scan_interval_ms)) {
        for (const auto& hit : integrity_->scan()) {
            report_violation({
                ViolationType::SelfIntegrityFailed,
                ViolationSeverity::Critical,
                "Integrity check failed",
                hit,
            });
        }
        last_integrity_scan_ = now;
    }
}

void Encryptic::register_memory_region(const MemoryRegion& region) {
    std::lock_guard lock(mutex_);
    if (memory_guard_) {
        memory_guard_->register_region(region);
    }
}

void Encryptic::report_violation(Violation violation) {
    if (config_.on_violation) {
        config_.on_violation(violation);
    }

    if (telemetry_) {
        telemetry_->queue_violation(violation);
    }

    if (screenshot_) {
        screenshot_->capture_on_violation(violation);
    }
}

} // namespace anticheat

extern "C" {

ENCRYPTIC_API void encryptic_start_default(void) {
    anticheat::Config cfg = anticheat::Config::from_preset(anticheat::ProtectionPreset::Balanced);
    anticheat::Encryptic::instance().start(cfg);
}

ENCRYPTIC_API int encryptic_start_from_file(const char* path) {
    if (path == nullptr) {
        return 0;
    }
    anticheat::Encryptic::instance().start_from_file(path);
    return anticheat::Encryptic::instance().is_running() ? 1 : 0;
}

ENCRYPTIC_API void encryptic_tick(void) {
    anticheat::Encryptic::instance().tick();
}

ENCRYPTIC_API void encryptic_stop(void) {
    anticheat::Encryptic::instance().stop();
}

ENCRYPTIC_API void encryptic_watermark_preview(const EncrypticWatermarkConfig* cfg) {
    anticheat::WatermarkConfig wc;
    if (cfg != nullptr) {
        wc.enabled = cfg->enabled != 0;
        if (cfg->game_title) {
            wc.game_title = cfg->game_title;
        }
        if (cfg->brand_title) {
            wc.brand_title = cfg->brand_title;
        }
        if (cfg->brand_subtitle) {
            wc.brand_subtitle = cfg->brand_subtitle;
        }
        if (cfg->status_text) {
            wc.status_text = cfg->status_text;
        }
        if (cfg->footer_text) {
            wc.footer_text = cfg->footer_text;
        }
        if (cfg->version_text) {
            wc.version_text = cfg->version_text;
        }
        wc.display_duration_ms = cfg->display_duration_ms ? cfg->display_duration_ms : 5200;
        wc.fade_in_ms = cfg->fade_in_ms ? cfg->fade_in_ms : 380;
        wc.fade_out_ms = cfg->fade_out_ms ? cfg->fade_out_ms : 650;
        wc.accent_r = cfg->accent_r;
        wc.accent_g = cfg->accent_g;
        wc.accent_b = cfg->accent_b;
        wc.show_progress_bar = cfg->show_progress_bar != 0;
        wc.show_logo_mark = cfg->show_logo_mark != 0;
        wc.fullscreen_dim = cfg->fullscreen_dim != 0;
        wc.allow_skip = cfg->allow_skip != 0;
    }
    anticheat::Watermark::show_preview(wc);
}

} // extern "C"
