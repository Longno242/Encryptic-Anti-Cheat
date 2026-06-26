#include <anticheat/c_api.h>
#include <anticheat/watermark.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_file(const char* path) {
    std::ifstream in(path);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
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
    return static_cast<unsigned>(std::stoul(json.substr(pos + 1)));
}

bool json_bool(const std::string& json, const std::string& key, bool fallback) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return fallback;
    }
    return json.find("true", pos) != std::string::npos;
}

void load_stages(const std::string& json, anticheat::WatermarkConfig& cfg) {
    const auto start = json.find("\"loading_stages\"");
    if (start == std::string::npos) {
        return;
    }
    auto pos = json.find('[', start);
    auto end = json.find(']', pos);
    if (pos == std::string::npos || end == std::string::npos) {
        return;
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
        cfg.loading_stages.push_back(block.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
}

anticheat::WatermarkConfig load_config() {
    anticheat::WatermarkConfig cfg;
    const std::string json = read_file("encryptic_watermark.json");
    if (json.empty()) {
        cfg.game_title = "Encryptic Preview";
        return cfg;
    }

    if (const auto v = json_string(json, "game_title"); !v.empty()) {
        cfg.game_title = v;
    }
    if (const auto v = json_string(json, "brand_title"); !v.empty()) {
        cfg.brand_title = v;
    } else if (const auto v = json_string(json, "brand_line_1"); !v.empty()) {
        cfg.brand_title = v;
    }
    if (const auto v = json_string(json, "brand_subtitle"); !v.empty()) {
        cfg.brand_subtitle = v;
    } else if (const auto v = json_string(json, "brand_line_2"); !v.empty()) {
        cfg.brand_subtitle = v;
    }
    if (const auto v = json_string(json, "status_text"); !v.empty()) {
        cfg.status_text = v;
    }
    if (const auto v = json_string(json, "footer_text"); !v.empty()) {
        cfg.footer_text = v;
    }
    if (const auto v = json_string(json, "version_text"); !v.empty()) {
        cfg.version_text = v;
    }

    cfg.enabled = json_bool(json, "enabled", true);
    cfg.show_progress_bar = json_bool(json, "show_progress_bar", true);
    cfg.show_logo_mark = json_bool(json, "show_logo_mark", true);
    cfg.fullscreen_dim = json_bool(json, "fullscreen_dim", false);
    cfg.allow_skip = json_bool(json, "allow_skip", false);
    cfg.display_duration_ms = json_uint(json, "display_duration_ms", 5200);
    cfg.fade_in_ms = json_uint(json, "fade_in_ms", 380);
    cfg.fade_out_ms = json_uint(json, "fade_out_ms", 520);
    load_stages(json, cfg);

    return cfg;
}

} // namespace

int main() {
    std::cout << "Encryptic Anti-Cheat - launch splash preview\n";
    std::cout << "Edit encryptic_watermark.json then rebuild or rerun.\n";
    std::cout << "Splash cannot be skipped - waits until complete.\n\n";

    anticheat::Watermark::show_preview(load_config());
    return 0;
}
