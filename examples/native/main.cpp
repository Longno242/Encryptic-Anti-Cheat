#include <anticheat/anticheat.hpp>
#include <iostream>

int main() {
    anticheat::Config cfg = anticheat::Config::from_preset(anticheat::ProtectionPreset::Balanced);
    cfg.watermark.game_title = "Encryptic Demo";

    cfg.on_violation = [](const anticheat::Violation& v) {
        std::cerr << "[Encryptic] " << v.message;
        if (!v.detail.empty()) {
            std::cerr << " — " << v.detail;
        }
        std::cerr << '\n';
    };

    anticheat::Encryptic::instance().start(cfg);
    std::cout << "Encryptic Anti-Cheat running. Press Enter to exit.\n";

    while (std::cin.get() != '\n') {
        anticheat::Encryptic::instance().tick();
    }

    anticheat::Encryptic::instance().stop();
    return 0;
}
