#include <anticheat/process_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace anticheat {

void ProcessGuard::configure(const std::vector<std::string>& blocked_names) {
    blocked_.clear();
    for (const auto& name : blocked_names) {
        blocked_.push_back(platform::to_lower(name));
    }
}

void ProcessGuard::scan() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            char name[MAX_PATH]{};
            WideCharToMultiByte(CP_UTF8, 0, entry.szExeFile, -1, name, MAX_PATH, nullptr, nullptr);
            const std::string lower = platform::to_lower(name);

            for (const auto& blocked : blocked_) {
                if (lower.find(blocked) != std::string::npos) {
                    Encryptic::instance().report_violation({
                        ViolationType::CheatProcessDetected,
                        ViolationSeverity::Critical,
                        "Blocked process running",
                        name,
                    });
                    break;
                }
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
}

} // namespace anticheat

#else

namespace anticheat {

void ProcessGuard::configure(const std::vector<std::string>&) {}
void ProcessGuard::scan() {}

} // namespace anticheat

#endif
