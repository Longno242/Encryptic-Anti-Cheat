#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace anticheat {

class HookGuard {
public:
    void prime();
    void scan();

private:
    struct ApiSample {
        std::string module;
        std::string export_name;
        void* address{};
        std::uint8_t bytes[16]{};
    };

    void sample_api(const char* module, const char* export_name, ApiSample& out);
    bool is_hooked(const ApiSample& original, void* current) const;

    std::unordered_map<std::string, ApiSample> baselines_;
};

} // namespace anticheat
