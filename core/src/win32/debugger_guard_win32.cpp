#include <anticheat/debugger_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

using NTSTATUS = LONG;
using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

constexpr ULONG kProcessDebugPort = 7;
constexpr ULONG kProcessDebugObjectHandle = 30;
constexpr ULONG kProcessDebugFlags = 31;

bool peb_being_debugged() {
#if defined(_M_X64) || defined(__x86_64__)
    const auto peb = reinterpret_cast<const unsigned char*>(__readgsqword(0x60));
    if (peb && peb[2] != 0) {
        return true;
    }
    const auto nt_global_flag = *reinterpret_cast<const DWORD*>(peb + 0xBC);
    return (nt_global_flag & 0x70) != 0;
#elif defined(_M_IX86) || defined(__i386__)
    const auto peb = reinterpret_cast<const unsigned char*>(__readfsdword(0x30));
    if (peb && peb[2] != 0) {
        return true;
    }
    const auto nt_global_flag = *reinterpret_cast<const DWORD*>(peb + 0x68);
    return (nt_global_flag & 0x70) != 0;
#else
    return false;
#endif
}

bool hardware_breakpoints_set() {
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx)) {
        return false;
    }
    return ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3;
}

bool output_debug_string_trap() {
    SetLastError(0);
    OutputDebugStringA("encryptic_probe");
    return GetLastError() != 0;
}

bool nt_debug_checks() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        return false;
    }
    auto* query = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (!query) {
        return false;
    }

    ULONG_PTR debug_port = 0;
    if (query(GetCurrentProcess(), kProcessDebugPort, &debug_port, sizeof(debug_port), nullptr) == 0 &&
        debug_port != 0) {
        return true;
    }

    HANDLE debug_object = nullptr;
    if (query(GetCurrentProcess(), kProcessDebugObjectHandle, &debug_object, sizeof(debug_object), nullptr) ==
            0 &&
        debug_object != nullptr) {
        return true;
    }

    ULONG debug_flags = 0;
    if (query(GetCurrentProcess(), kProcessDebugFlags, &debug_flags, sizeof(debug_flags), nullptr) == 0 &&
        debug_flags == 0) {
        return true;
    }

    return false;
}

bool contains_icase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return false;
    }
    auto lower = haystack;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower.find(needle) != std::string::npos;
}

bool scan_processes(const std::vector<std::string>& blocked, std::string& hit) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snap, &entry)) {
        do {
            char name[MAX_PATH]{};
            WideCharToMultiByte(CP_UTF8, 0, entry.szExeFile, -1, name, MAX_PATH, nullptr, nullptr);
            for (const auto& blocked_name : blocked) {
                if (contains_icase(name, blocked_name)) {
                    hit = name;
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

bool scan_services(const std::vector<std::string>& blocked, std::string& hit) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) {
        return false;
    }

    DWORD bytes_needed = 0;
    DWORD services_count = 0;
    EnumServicesStatusExW(
        scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE, nullptr, 0, &bytes_needed,
        &services_count, nullptr, nullptr);

    std::vector<BYTE> buffer(bytes_needed);
    auto* services = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
    if (!EnumServicesStatusExW(
            scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE, buffer.data(), bytes_needed,
            &bytes_needed, &services_count, nullptr, nullptr)) {
        CloseServiceHandle(scm);
        return false;
    }

    bool found = false;
    for (DWORD i = 0; i < services_count; ++i) {
        char name[512]{};
        WideCharToMultiByte(
            CP_UTF8, 0, services[i].lpServiceName, -1, name, sizeof(name), nullptr, nullptr);
        char display[512]{};
        WideCharToMultiByte(
            CP_UTF8, 0, services[i].lpDisplayName, -1, display, sizeof(display), nullptr, nullptr);

        for (const auto& blocked_name : blocked) {
            if (contains_icase(name, blocked_name) || contains_icase(display, blocked_name)) {
                hit = std::string(name) + " (" + display + ")";
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
    }

    CloseServiceHandle(scm);
    return found;
}

struct WindowCtx {
    std::vector<std::string> blocked;
    std::string hit;
};

BOOL CALLBACK enum_debug_windows(HWND hwnd, LPARAM param) {
    auto* ctx = reinterpret_cast<WindowCtx*>(param);
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, 512);
    char utf8[512]{};
    WideCharToMultiByte(CP_UTF8, 0, title, -1, utf8, sizeof(utf8), nullptr, nullptr);

    for (const auto& b : ctx->blocked) {
        if (contains_icase(utf8, b)) {
            ctx->hit = utf8;
            return FALSE;
        }
    }
    return TRUE;
}

bool scan_debugger_windows(const std::vector<std::string>& blocked, std::string& hit) {
    WindowCtx ctx;
    ctx.blocked = blocked;
    ctx.blocked.insert(ctx.blocked.end(), {"x64dbg", "x32dbg", "ida", "ollydbg", "windbg", "cheat engine"});
    EnumWindows(enum_debug_windows, reinterpret_cast<LPARAM>(&ctx));
    if (!ctx.hit.empty()) {
        hit = ctx.hit;
        return true;
    }
    return false;
}

bool scan_software_breakpoints() {
    HMODULE mod = GetModuleHandleA("encryptic_core.dll");
    if (!mod) {
        mod = GetModuleHandleA(nullptr);
    }
    if (!mod) {
        return false;
    }

    const auto* base = reinterpret_cast<const std::uint8_t*>(mod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (std::memcmp(section[i].Name, ".text", 5) != 0) {
            continue;
        }

        const auto* text = base + section[i].VirtualAddress;
        const std::size_t size = section[i].Misc.VirtualSize;
        std::size_t hits = 0;
        for (std::size_t j = 0; j < size; ++j) {
            if (text[j] == 0xCC) {
                ++hits;
                if (hits >= 3) {
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace

namespace anticheat {

void DebuggerGuard::configure(const std::vector<std::string>& blocked_services,
                              const std::vector<std::string>& blocked_processes) {
    blocked_services_ = blocked_services;
    blocked_processes_ = blocked_processes;
    if (blocked_processes_.empty()) {
        blocked_processes_ = {
            "cheatengine", "x64dbg", "x32dbg", "ida", "ida64", "ollydbg", "windbg", "immunitydebugger",
            "processhacker", "httpdebugger",
        };
    }
    if (blocked_services_.empty()) {
        blocked_services_ = {"KProcessHacker3", "KSystemInformer", "HttpDebuggerPro", "x64dbg"};
    }
}

std::vector<std::string> DebuggerGuard::scan(bool terminate_on_detect) {
    std::vector<std::string> hits;

    if (IsDebuggerPresent()) {
        hits.push_back("IsDebuggerPresent");
    }

    BOOL remote = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote) && remote) {
        hits.push_back("CheckRemoteDebuggerPresent");
    }

    if (peb_being_debugged()) {
        hits.push_back("PEB debug flags");
    }

    if (hardware_breakpoints_set()) {
        hits.push_back("Hardware breakpoints");
    }

    if (output_debug_string_trap()) {
        hits.push_back("OutputDebugString trap");
    }

    if (nt_debug_checks()) {
        hits.push_back("NtQueryInformationProcess debug port/object");
    }

    if (scan_software_breakpoints()) {
        hits.push_back("Software breakpoints in protected module");
    }

    std::string proc_hit;
    if (scan_processes(blocked_processes_, proc_hit)) {
        hits.push_back("Debugger process: " + proc_hit);
    }

    std::string svc_hit;
    if (scan_services(blocked_services_, svc_hit)) {
        hits.push_back("Debugger service: " + svc_hit);
    }

    std::string win_hit;
    if (scan_debugger_windows(blocked_processes_, win_hit)) {
        hits.push_back("Debugger window: " + win_hit);
    }

    if (!hits.empty() && terminate_on_detect) {
        TerminateProcess(GetCurrentProcess(), 0xACE);
    }

    return hits;
}

} // namespace anticheat

#else

namespace anticheat {

void DebuggerGuard::configure(const std::vector<std::string>&, const std::vector<std::string>&) {}

std::vector<std::string> DebuggerGuard::scan(bool) {
    return {};
}

} // namespace anticheat

#endif
