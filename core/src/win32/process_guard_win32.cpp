#include <anticheat/process_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !wide[0]) {
        return {};
    }
    char utf8[MAX_PATH * 2]{};
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

bool get_process_image_path(DWORD pid, std::string& out) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return false;
    }

    wchar_t path[MAX_PATH * 2]{};
    DWORD size = static_cast<DWORD>(std::size(path));
    const BOOL ok = QueryFullProcessImageNameW(proc, 0, path, &size);
    CloseHandle(proc);
    if (!ok || size == 0) {
        return false;
    }

    out = wide_to_utf8(path);
    return !out.empty();
}

bool contains_blocked(const std::string& value, const std::vector<std::string>& blocked) {
    const std::string lower = anticheat::platform::to_lower(value);
    for (const auto& blocked_name : blocked) {
        if (lower.find(anticheat::platform::to_lower(blocked_name)) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

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
            if (entry.th32ProcessID <= 4) {
                continue;
            }

            const std::string exe_name = wide_to_utf8(entry.szExeFile);
            std::string full_path;
            get_process_image_path(entry.th32ProcessID, full_path);

            std::string hit;
            if (contains_blocked(exe_name, blocked_)) {
                hit = exe_name;
            } else if (!full_path.empty() && contains_blocked(full_path, blocked_)) {
                hit = full_path;
            }

            if (!hit.empty()) {
                if (reported_.count(hit) == 0) {
                    reported_.insert(hit);
                    Encryptic::instance().report_violation({
                        ViolationType::CheatProcessDetected,
                        ViolationSeverity::Critical,
                        "Blocked process running",
                        hit,
                    });
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
