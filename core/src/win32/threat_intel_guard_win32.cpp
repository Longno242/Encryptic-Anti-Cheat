#include <anticheat/threat_intel_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

bool contains_icase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return false;
    }
    return anticheat::platform::to_lower(haystack).find(anticheat::platform::to_lower(needle)) != std::string::npos;
}

std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !wide[0]) {
        return {};
    }
    char utf8[MAX_PATH * 2]{};
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    wchar_t wide[MAX_PATH * 2]{};
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide, MAX_PATH * 2);
    return wide;
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

bool scan_process_modules(DWORD pid, const std::vector<std::string>& blocked, std::string& hit) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!proc) {
        return false;
    }

    HMODULE modules[512]{};
    DWORD needed = 0;
    if (!EnumProcessModules(proc, modules, sizeof(modules), &needed)) {
        CloseHandle(proc);
        return false;
    }

    const DWORD count = needed / sizeof(HMODULE);
    bool found = false;

    for (DWORD i = 0; i < count && i < 512; ++i) {
        wchar_t mod_name[MAX_PATH]{};
        if (!GetModuleBaseNameW(proc, modules[i], mod_name, MAX_PATH)) {
            continue;
        }

        const std::string name = wide_to_utf8(mod_name);
        for (const auto& blocked_name : blocked) {
            if (contains_icase(name, blocked_name)) {
                std::string proc_path;
                get_process_image_path(pid, proc_path);
                hit = name + " in " + (proc_path.empty() ? std::to_string(pid) : proc_path);
                found = true;
                break;
            }
        }

        if (found) {
            break;
        }

        wchar_t mod_path[MAX_PATH * 2]{};
        if (GetModuleFileNameExW(proc, modules[i], mod_path, MAX_PATH * 2)) {
            const std::string path = wide_to_utf8(mod_path);
            for (const auto& blocked_name : blocked) {
                if (contains_icase(path, blocked_name)) {
                    std::string proc_path;
                    get_process_image_path(pid, proc_path);
                    hit = path + " (process: " + (proc_path.empty() ? std::to_string(pid) : proc_path) + ")";
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            break;
        }
    }

    CloseHandle(proc);
    return found;
}

bool scan_all_process_modules(const std::vector<std::string>& blocked, std::string& hit) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID <= 4) {
                continue;
            }
            if (scan_process_modules(entry.th32ProcessID, blocked, hit)) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return found;
}

bool scan_process_paths(const std::vector<std::string>& segments, std::string& hit) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID <= 4) {
                continue;
            }

            std::string path;
            if (!get_process_image_path(entry.th32ProcessID, path)) {
                continue;
            }

            for (const auto& segment : segments) {
                if (contains_icase(path, segment)) {
                    hit = path;
                    found = true;
                    break;
                }
            }

            if (found) {
                break;
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return found;
}

bool scan_ce_drivers(const std::vector<std::string>& blocked, std::string& hit) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD index = 0;
    wchar_t name[256]{};
    DWORD name_len = 256;
    bool found = false;

    while (RegEnumKeyExW(key, index++, name, &name_len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        const std::string service = wide_to_utf8(name);
        for (const auto& blocked_driver : blocked) {
            if (contains_icase(service, blocked_driver)) {
                hit = service;
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
        name_len = 256;
    }

    RegCloseKey(key);
    return found;
}

struct WindowScanCtx {
    std::vector<std::string> blocked_classes;
    std::vector<std::string> blocked_titles;
    std::string hit;
    std::string kind;
};

BOOL CALLBACK enum_threat_windows(HWND hwnd, LPARAM param) {
    auto* ctx = reinterpret_cast<WindowScanCtx*>(param);
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t class_name[256]{};
    wchar_t title[512]{};
    GetClassNameW(hwnd, class_name, 256);
    GetWindowTextW(hwnd, title, 512);

    const std::string cls = wide_to_utf8(class_name);
    const std::string ttl = wide_to_utf8(title);

    for (const auto& blocked : ctx->blocked_classes) {
        if (contains_icase(cls, blocked)) {
            ctx->hit = cls + (ttl.empty() ? "" : " — " + ttl);
            ctx->kind = "class";
            return FALSE;
        }
    }

    for (const auto& blocked : ctx->blocked_titles) {
        if (contains_icase(ttl, blocked)) {
            ctx->hit = ttl + (cls.empty() ? "" : " [" + cls + "]");
            ctx->kind = "title";
            return FALSE;
        }
    }

    return TRUE;
}

bool scan_suspicious_windows(const std::vector<std::string>& classes,
                             const std::vector<std::string>& titles,
                             std::string& hit,
                             std::string& kind) {
    WindowScanCtx ctx;
    ctx.blocked_classes = classes;
    ctx.blocked_titles = titles;
    EnumWindows(enum_threat_windows, reinterpret_cast<LPARAM>(&ctx));
    if (!ctx.hit.empty()) {
        hit = ctx.hit;
        kind = ctx.kind;
        return true;
    }
    return false;
}

bool scan_version_info_heuristics(std::string& hit) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    static const char* suspicious_descriptions[] = {
        "cheat engine",
        "memory scanner",
        "debugger",
        "process hacker",
        "artmoney",
    };

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID <= 4) {
                continue;
            }

            std::string path;
            if (!get_process_image_path(entry.th32ProcessID, path)) {
                continue;
            }

            const std::wstring wide = utf8_to_wide(path);
            DWORD dummy = 0;
            const DWORD ver_size = GetFileVersionInfoSizeW(wide.c_str(), &dummy);
            if (ver_size == 0) {
                continue;
            }

            std::vector<BYTE> buffer(ver_size);
            if (!GetFileVersionInfoW(wide.c_str(), 0, ver_size, buffer.data())) {
                continue;
            }

            struct LANGANDCODEPAGE {
                WORD wLanguage;
                WORD wCodePage;
            } *translate = nullptr;
            UINT translate_len = 0;
            if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&translate),
                                &translate_len) ||
                translate_len < sizeof(LANGANDCODEPAGE)) {
                continue;
            }

            wchar_t sub_block[64]{};
            swprintf_s(sub_block, L"\\StringFileInfo\\%04x%04x\\FileDescription", translate[0].wLanguage,
                       translate[0].wCodePage);

            wchar_t* description = nullptr;
            UINT desc_len = 0;
            if (!VerQueryValueW(buffer.data(), sub_block, reinterpret_cast<LPVOID*>(&description), &desc_len) ||
                !description) {
                continue;
            }

            const std::string desc = wide_to_utf8(description);
            for (const auto* suspicious : suspicious_descriptions) {
                if (contains_icase(desc, suspicious)) {
                    hit = path + " — " + desc;
                    found = true;
                    break;
                }
            }

            if (found) {
                break;
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return found;
}

} // namespace

namespace anticheat {

void ThreatIntelGuard::configure(const std::vector<std::string>& blocked_modules,
                                 const std::vector<std::string>& blocked_window_classes,
                                 const std::vector<std::string>& blocked_window_titles,
                                 const std::vector<std::string>& blocked_path_segments,
                                 const std::vector<std::string>& blocked_drivers) {
    blocked_modules_ = blocked_modules;
    if (blocked_modules_.empty()) {
        blocked_modules_ = {
            "dbk32.dll", "dbk64.dll", "cedebug.dll", "vehdebug.dll", "speedhack.dll",
            "luaclient.dll", "cehook.dll", "ceserver.dll", "monodatacollector.dll",
            "blackbone.dll", "injector.dll", "xenos.dll", "extremeinjector",
        };
    }

    blocked_window_classes_ = blocked_window_classes;
    if (blocked_window_classes_.empty()) {
        blocked_window_classes_ = {
            "TfrmCheatEngine", "TfrmAutoInject", "TfrmMemoryRecords", "TfrmLuaEngine",
            "TfrmStructures2", "TCETrainer", "TfrmAddresslist", "TfrmProcessWatcher",
            "TfrmBreakpointlist", "TCheatForm", "TMemoryBrowser",
        };
    }

    blocked_window_titles_ = blocked_window_titles;
    if (blocked_window_titles_.empty()) {
        blocked_window_titles_ = {
            "cheat engine", "memory view", "add address", "pointer scan", "speedhack",
            "artmoney", "process hacker", "x64dbg", "extreme injector", "xenos injector",
            "guided hacking", "wemod", "trainer", "memory scanner", "value scan",
            "reclass.net", "tsearch", "game guardian",
        };
    }

    blocked_path_segments_ = blocked_path_segments;
    if (blocked_path_segments_.empty()) {
        blocked_path_segments_ = {
            "\\cheat engine\\", "\\cheatengine\\", "\\ce tables\\", "\\trainers\\",
            "\\injectors\\", "\\x64dbg\\", "\\process hacker\\", "\\artmoney\\",
            "\\wemod\\", "\\guidedhacking\\", "\\extreme injector\\",
        };
    }

    blocked_drivers_ = blocked_drivers;
    if (blocked_drivers_.empty()) {
        blocked_drivers_ = {
            "dbk64", "dbk32", "CEDRIVER", "CEDRIVER73", "CEDRIVER74", "kprocesshacker",
            "kprocesshacker3", "ksysteminformer", "dbvm",
        };
    }
}

void ThreatIntelGuard::report_once(const std::string& key, const std::string& message,
                                   const std::string& detail) {
    if (reported_.count(key) != 0) {
        return;
    }
    reported_.insert(key);

    Encryptic::instance().report_violation({
        ViolationType::ThreatToolDetected,
        ViolationSeverity::Critical,
        message,
        detail,
    });
}

void ThreatIntelGuard::scan() {
    std::string hit;

    if (scan_all_process_modules(blocked_modules_, hit)) {
        report_once("module:" + hit, "Cheat tool module detected", hit);
    }

    if (scan_process_paths(blocked_path_segments_, hit)) {
        report_once("path:" + hit, "Cheat tool path detected", hit);
    }

    if (scan_ce_drivers(blocked_drivers_, hit)) {
        report_once("driver:" + hit, "Cheat engine driver installed", hit);
    }

    std::string kind;
    if (scan_suspicious_windows(blocked_window_classes_, blocked_window_titles_, hit, kind)) {
        report_once("window:" + hit, kind == "class" ? "Cheat tool window class detected"
                                                     : "Cheat tool window title detected",
                    hit);
    }

    if (scan_version_info_heuristics(hit)) {
        report_once("version:" + hit, "Cheat tool binary metadata detected", hit);
    }
}

} // namespace anticheat

#else

namespace anticheat {

void ThreatIntelGuard::configure(const std::vector<std::string>&, const std::vector<std::string>&,
                                 const std::vector<std::string>&, const std::vector<std::string>&,
                                 const std::vector<std::string>&) {}

void ThreatIntelGuard::scan() {}

} // namespace anticheat

#endif
