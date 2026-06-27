#include <anticheat/config.hpp>

namespace anticheat {

Config Config::from_preset(ProtectionPreset preset) {
    Config cfg;
    cfg.apply_preset(preset);
    return cfg;
}

void Config::apply_preset(ProtectionPreset p) {
    preset = p;
    switch (p) {
    case ProtectionPreset::Minimal:
        dll_guard = true;
        debugger_guard = false;
        hook_guard = false;
        memory_guard = false;
        module_whitelist = false;
        thread_guard = false;
        timing_guard = true;
        handle_guard = false;
        process_guard = true;
        threat_intel_guard = true;
        injection_guard = true;
        anti_bypass_guard = false;
        hash_guard = false;
        pipe_guard = true;
        kernel_guard = false;
        vm_guard = false;
        overlay_guard = false;
        input_guard = false;
        watchdog_guard = true;
        anti_tamper = true;
        watermark.enabled = true;
        break;
    case ProtectionPreset::Balanced:
        dll_guard = true;
        debugger_guard = true;
        hook_guard = true;
        memory_guard = true;
        module_whitelist = false;
        thread_guard = true;
        timing_guard = true;
        handle_guard = true;
        process_guard = true;
        threat_intel_guard = true;
        injection_guard = true;
        anti_bypass_guard = true;
        hash_guard = true;
        pipe_guard = true;
        kernel_guard = false;
        vm_guard = false;
        overlay_guard = true;
        input_guard = true;
        watchdog_guard = true;
        anti_tamper = true;
        integrity.self_integrity = true;
        watermark.enabled = true;
        break;
    case ProtectionPreset::Aggressive:
        dll_guard = true;
        debugger_guard = true;
        hook_guard = true;
        memory_guard = true;
        module_whitelist = true;
        thread_guard = true;
        timing_guard = true;
        handle_guard = true;
        process_guard = true;
        threat_intel_guard = true;
        injection_guard = true;
        anti_bypass_guard = true;
        hash_guard = true;
        pipe_guard = true;
        kernel_guard = true;
        vm_guard = true;
        overlay_guard = true;
        input_guard = true;
        watchdog_guard = true;
        anti_tamper = true;
        integrity.self_integrity = true;
        integrity.game_file_integrity = true;
        screenshot.on_critical = true;
        block_unknown_dlls = true;
        require_signature = true;
        kick_on_critical = true;
        watermark.enabled = true;
        break;
    case ProtectionPreset::Custom:
    default:
        break;
    }
}

} // namespace anticheat
