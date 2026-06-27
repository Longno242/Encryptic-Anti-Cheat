#include <anticheat/injection_guard.hpp>
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
#include <vector>

namespace {

bool contains_blocked(const std::string& value, const std::vector<std::string>& blocked) {
    const std::string lower = anticheat::platform::to_lower(value);
    for (const auto& b : blocked) {
        if (lower.find(anticheat::platform::to_lower(b)) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !wide[0]) {
        return {};
    }
    char utf8[MAX_PATH * 2]{};
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

bool get_process_path(DWORD pid, std::string& out) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return false;
    }
    wchar_t path[MAX_PATH * 2]{};
    DWORD size = static_cast<DWORD>(std::size(path));
    const BOOL ok = QueryFullProcessImageNameW(proc, 0, path, &size);
    CloseHandle(proc);
    if (!ok) {
        return false;
    }
    out = wide_to_utf8(path);
    return !out.empty();
}

DWORD get_parent_pid(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD parent = 0;

    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                parent = entry.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return parent;
}

bool is_suspicious_load_path(const std::string& path) {
    const std::string lower = anticheat::platform::to_lower(path);
    static const char* markers[] = {
        "\\temp\\", "\\tmp\\", "\\appdata\\local\\temp\\", "\\downloads\\",
        "\\injector\\", "\\cheat engine\\", "\\cheatengine\\",
    };
    for (const auto* m : markers) {
        if (lower.find(m) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace anticheat {

void InjectionGuard::configure(const std::vector<std::string>& blocked_processes) {
    blocked_ = blocked_processes;
}

void InjectionGuard::report_once(const std::string& key, const std::string& detail) {
    if (reported_.count(key) != 0) {
        return;
    }
    reported_.insert(key);
    Encryptic::instance().report_violation({
        ViolationType::InjectionDetected,
        ViolationSeverity::Critical,
        "Injection or suspicious loader detected",
        detail,
    });
}

void InjectionGuard::scan() {
    DWORD pid = GetCurrentProcessId();
    for (int depth = 0; depth < 4; ++depth) {
        const DWORD parent = get_parent_pid(pid);
        if (parent == 0 || parent == pid) {
            break;
        }

        std::string path;
        if (get_process_path(parent, path) && contains_blocked(path, blocked_)) {
            report_once("parent:" + path, "Suspicious parent process: " + path);
            break;
        }

        pid = parent;
    }

    HMODULE modules[1024]{};
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
        return;
    }

    const DWORD count = needed / sizeof(HMODULE);
    for (DWORD i = 0; i < count && i < 1024; ++i) {
        char path[MAX_PATH * 2]{};
        if (GetModuleFileNameA(modules[i], path, sizeof(path)) == 0) {
            continue;
        }

        const std::string mod_path(path);
        if (!is_suspicious_load_path(mod_path)) {
            continue;
        }

        if (platform::verify_authenticode(mod_path)) {
            continue;
        }

        const std::string key = "mod:" + mod_path;
        if (reported_.count(key) == 0) {
            report_once(key, "Unsigned module from suspicious path: " + mod_path);
        }
    }
}

} // namespace anticheat

#else

namespace anticheat {

void InjectionGuard::configure(const std::vector<std::string>&) {}
void InjectionGuard::scan() {}

} // namespace anticheat

#endif
