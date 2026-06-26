#include <anticheat/config.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace anticheat {
namespace {

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string json_string(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return {};
    }
    pos = json.find('"', pos);
    if (pos == std::string::npos) {
        return {};
    }
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) {
        return {};
    }
    return json.substr(pos + 1, end - pos - 1);
}

bool json_bool(const std::string& json, const std::string& key, bool fallback) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return fallback;
    }
    const auto true_pos = json.find("true", pos);
    const auto false_pos = json.find("false", pos);
    if (true_pos == std::string::npos && false_pos == std::string::npos) {
        return fallback;
    }
    if (true_pos == std::string::npos) {
        return false;
    }
    if (false_pos == std::string::npos) {
        return true;
    }
    return true_pos < false_pos;
}

unsigned json_uint(const std::string& json, const std::string& key, unsigned fallback) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return fallback;
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return fallback;
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    std::size_t end = pos;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
        ++end;
    }
    if (end == pos) {
        return fallback;
    }
    return static_cast<unsigned>(std::stoul(json.substr(pos, end - pos)));
}

std::string extract_block(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    pos = json.find('{', pos);
    if (pos == std::string::npos) {
        return {};
    }
    int depth = 0;
    for (std::size_t i = pos; i < json.size(); ++i) {
        if (json[i] == '{') {
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(pos, i - pos + 1);
            }
        }
    }
    return {};
}

std::vector<std::string> json_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> out;
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return out;
    }
    pos = json.find('[', pos);
    auto end = json.find(']', pos);
    if (pos == std::string::npos || end == std::string::npos) {
        return out;
    }
    std::string block = json.substr(pos + 1, end - pos - 1);
    std::size_t i = 0;
    while (i < block.size()) {
        auto q1 = block.find('"', i);
        if (q1 == std::string::npos) {
            break;
        }
        auto q2 = block.find('"', q1 + 1);
        if (q2 == std::string::npos) {
            break;
        }
        out.push_back(block.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return out;
}

ProtectionPreset parse_preset(const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "minimal") {
        return ProtectionPreset::Minimal;
    }
    if (lower == "aggressive") {
        return ProtectionPreset::Aggressive;
    }
    if (lower == "custom") {
        return ProtectionPreset::Custom;
    }
    return ProtectionPreset::Balanced;
}

void load_watermark(const std::string& block, WatermarkConfig& wm) {
    if (block.empty()) {
        return;
    }
    wm.enabled = json_bool(block, "enabled", wm.enabled);
    wm.show_on_launch = json_bool(block, "show_on_launch", wm.show_on_launch);
    if (const auto v = json_string(block, "game_title"); !v.empty()) {
        wm.game_title = v;
    }
    if (const auto v = json_string(block, "brand_title"); !v.empty()) {
        wm.brand_title = v;
    }
    if (const auto v = json_string(block, "brand_subtitle"); !v.empty()) {
        wm.brand_subtitle = v;
    }
    wm.display_duration_ms = json_uint(block, "display_duration_ms", wm.display_duration_ms);
}

void load_telemetry(const std::string& block, TelemetryConfig& t) {
    if (block.empty()) {
        return;
    }
    t.enabled = json_bool(block, "enabled", t.enabled);
    if (const auto v = json_string(block, "endpoint_url"); !v.empty()) {
        t.endpoint_url = v;
    }
    if (const auto v = json_string(block, "api_key"); !v.empty()) {
        t.api_key = v;
    }
    if (const auto v = json_string(block, "session_id"); !v.empty()) {
        t.session_id = v;
    }
    t.flush_interval_ms = json_uint(block, "flush_interval_ms", t.flush_interval_ms);
}

void load_heartbeat(const std::string& block, HeartbeatConfig& h) {
    if (block.empty()) {
        return;
    }
    h.enabled = json_bool(block, "enabled", h.enabled);
    if (const auto v = json_string(block, "endpoint_url"); !v.empty()) {
        h.endpoint_url = v;
    }
    if (const auto v = json_string(block, "session_token"); !v.empty()) {
        h.session_token = v;
    }
    if (const auto v = json_string(block, "hmac_secret"); !v.empty()) {
        h.hmac_secret = v;
    }
    h.interval_ms = json_uint(block, "interval_ms", h.interval_ms);
    h.miss_threshold = json_uint(block, "miss_threshold", h.miss_threshold);
}

void load_integrity(const std::string& block, IntegrityConfig& ig) {
    if (block.empty()) {
        return;
    }
    ig.self_integrity = json_bool(block, "self_integrity", ig.self_integrity);
    ig.game_file_integrity = json_bool(block, "game_file_integrity", ig.game_file_integrity);
}

void load_screenshot(const std::string& block, ScreenshotConfig& sc) {
    if (block.empty()) {
        return;
    }
    sc.on_critical = json_bool(block, "on_critical", sc.on_critical);
    if (const auto v = json_string(block, "output_dir"); !v.empty()) {
        sc.output_dir = v;
    }
}

} // namespace

bool load_config_from_json(const std::string& json, Config& out) {
    if (json.empty()) {
        return false;
    }

    out = Config::from_preset(ProtectionPreset::Balanced);
    out.preset = parse_preset(json_string(json, "preset"));
    if (out.preset != ProtectionPreset::Custom) {
        out.apply_preset(out.preset);
    }

    const auto guards = extract_block(json, "guards");
    if (!guards.empty()) {
        out.dll_guard = json_bool(guards, "dll_guard", out.dll_guard);
        out.debugger_guard = json_bool(guards, "debugger_guard", out.debugger_guard);
        out.hook_guard = json_bool(guards, "hook_guard", out.hook_guard);
        out.memory_guard = json_bool(guards, "memory_guard", out.memory_guard);
        out.module_whitelist = json_bool(guards, "module_whitelist", out.module_whitelist);
        out.thread_guard = json_bool(guards, "thread_guard", out.thread_guard);
        out.timing_guard = json_bool(guards, "timing_guard", out.timing_guard);
        out.handle_guard = json_bool(guards, "handle_guard", out.handle_guard);
        out.process_guard = json_bool(guards, "process_guard", out.process_guard);
        out.vm_guard = json_bool(guards, "vm_guard", out.vm_guard);
        out.overlay_guard = json_bool(guards, "overlay_guard", out.overlay_guard);
        out.input_guard = json_bool(guards, "input_guard", out.input_guard);
        out.watchdog_guard = json_bool(guards, "watchdog_guard", out.watchdog_guard);
        out.anti_tamper = json_bool(guards, "anti_tamper", out.anti_tamper);
    }

    const auto intervals = extract_block(json, "intervals_ms");
    if (!intervals.empty()) {
        out.hook_scan_interval_ms = json_uint(intervals, "hook_scan", out.hook_scan_interval_ms);
        out.memory_scan_interval_ms = json_uint(intervals, "memory_scan", out.memory_scan_interval_ms);
        out.thread_scan_interval_ms = json_uint(intervals, "thread_scan", out.thread_scan_interval_ms);
        out.handle_scan_interval_ms = json_uint(intervals, "handle_scan", out.handle_scan_interval_ms);
        out.timing_check_interval_ms = json_uint(intervals, "timing_check", out.timing_check_interval_ms);
        out.process_scan_interval_ms = json_uint(intervals, "process_scan", out.process_scan_interval_ms);
        out.overlay_scan_interval_ms = json_uint(intervals, "overlay_scan", out.overlay_scan_interval_ms);
        out.input_scan_interval_ms = json_uint(intervals, "input_scan", out.input_scan_interval_ms);
        out.watchdog_interval_ms = json_uint(intervals, "watchdog", out.watchdog_interval_ms);
        out.integrity_scan_interval_ms = json_uint(intervals, "integrity_scan", out.integrity_scan_interval_ms);
    }

    const auto blocked = json_string_array(json, "blocked_processes");
    if (!blocked.empty()) {
        out.blocked_processes = blocked;
    }
    const auto services = json_string_array(json, "debugger_blocked_services");
    if (!services.empty()) {
        out.debugger_blocked_services = services;
    }
    const auto allowed = json_string_array(json, "allowed_modules");
    if (!allowed.empty()) {
        out.allowed_modules = allowed;
    }
    const auto overlays = json_string_array(json, "overlay_whitelist");
    if (!overlays.empty()) {
        out.overlay_whitelist = overlays;
    }

    load_watermark(extract_block(json, "watermark"), out.watermark);
    load_telemetry(extract_block(json, "telemetry"), out.telemetry);
    load_heartbeat(extract_block(json, "heartbeat"), out.heartbeat);
    load_integrity(extract_block(json, "integrity"), out.integrity);
    load_screenshot(extract_block(json, "screenshot"), out.screenshot);

    if (const auto sid = json_string(json, "session_id"); !sid.empty() && sid != "auto-generated-at-runtime") {
        out.telemetry.session_id = sid;
    }

    return true;
}

bool load_config_from_file(const std::string& path, Config& out) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return load_config_from_json(ss.str(), out);
}

} // namespace anticheat
