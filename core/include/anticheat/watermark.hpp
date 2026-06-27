#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace anticheat {

struct WatermarkConfig {
    bool enabled{true};
    bool show_on_launch{true};
    bool blocking{true};
    bool topmost{true};
    bool fullscreen_dim{false};
    bool show_progress_bar{true};
    bool show_logo_mark{true};
    bool allow_skip{false};

    std::uint32_t display_duration_ms{5200};
    std::uint32_t fade_in_ms{380};
    std::uint32_t fade_out_ms{520};

    int card_width{520};
    int card_height{200};

    std::string game_title{"Your Game"};
    std::string brand_title{"Encryptic"};
    std::string brand_subtitle{"Anti-Cheat"};
    std::string status_text{"Starting protection service"};
    std::string footer_text{"All rights reserved. Encryptic Ltd."};
    std::string version_text{"1.0.0"};

    std::uint8_t accent_r{74};
    std::uint8_t accent_g{158};
    std::uint8_t accent_b{255};

    std::uint8_t card_r{24};
    std::uint8_t card_g{25};
    std::uint8_t card_b{28};

    std::vector<std::string> loading_stages;
};

class Watermark {
public:
    static void show_blocking(const WatermarkConfig& config);
    static void show_preview(const WatermarkConfig& config);
};

} // namespace anticheat
