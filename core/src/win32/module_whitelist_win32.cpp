#include <anticheat/module_whitelist.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

namespace anticheat {

void ModuleWhitelist::configure(const std::vector<std::string>& allowed, bool require_signature) {
    allowed_.clear();
    for (const auto& m : allowed) {
        allowed_.insert(platform::to_lower(platform::basename(m)));
    }
    require_signature_ = require_signature;
}

void ModuleWhitelist::establish_baseline() {
    baseline_.clear();
    HMODULE handles[1024]{};
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles), &needed)) {
        return;
    }

    const unsigned count = needed / sizeof(HMODULE);
    for (unsigned i = 0; i < count; ++i) {
        char path[MAX_PATH]{};
        if (GetModuleFileNameA(handles[i], path, MAX_PATH) == 0) {
            continue;
        }
        baseline_.insert(platform::to_lower(platform::basename(path)));
    }
}

void ModuleWhitelist::scan() {
    HMODULE handles[1024]{};
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles), &needed)) {
        return;
    }

    const unsigned count = needed / sizeof(HMODULE);
    for (unsigned i = 0; i < count; ++i) {
        char path[MAX_PATH]{};
        if (GetModuleFileNameA(handles[i], path, MAX_PATH) == 0) {
            continue;
        }

        const std::string name = platform::to_lower(platform::basename(path));
        if (baseline_.count(name) > 0) {
            continue;
        }

        baseline_.insert(name);

        if (!allowed_.count(name)) {
            Encryptic::instance().report_violation({
                ViolationType::ModuleNotWhitelisted,
                ViolationSeverity::Critical,
                "Module not in whitelist",
                path,
            });
            continue;
        }

        if (require_signature_ && !platform::verify_authenticode(path)) {
            Encryptic::instance().report_violation({
                ViolationType::ModuleNotWhitelisted,
                ViolationSeverity::High,
                "Whitelisted module lacks valid signature",
                path,
            });
        }
    }
}

} // namespace anticheat

#else

namespace anticheat {

void ModuleWhitelist::configure(const std::vector<std::string>&, bool) {}
void ModuleWhitelist::establish_baseline() {}
void ModuleWhitelist::scan() {}

} // namespace anticheat

#endif
