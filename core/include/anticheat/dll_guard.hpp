#pragma once

#include <anticheat/types.hpp>
#include <string>
#include <unordered_set>
#include <vector>

namespace anticheat {

struct LoadedModule {
    std::string path;
    std::string name;
    void* base{};
    std::size_t size{};
    bool signed_module{false};
};

class DllGuard {
public:
    void start(bool block_unknown, const std::vector<std::string>& allowed);
    void scan();
    bool install_load_library_hook();
    void remove_load_library_hook();

private:
    std::vector<LoadedModule> snapshot_modules() const;
    bool is_allowed(const std::string& name) const;

    std::unordered_set<std::string> baseline_;
    std::unordered_set<std::string> allowed_;
    bool block_unknown_{false};
    bool hook_installed_{false};
};

} // namespace anticheat
