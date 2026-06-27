#include <anticheat/anti_bypass_guard.hpp>
#include <anticheat/encryptic.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace {

using NTSTATUS = LONG;
using NtQuerySystemInformation_t = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);
using NtQueryInformationThread_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

constexpr ULONG kSystemKernelDebuggerInformation = 35;
constexpr ULONG kThreadHideFromDebugger = 17;

struct SYSTEM_KERNEL_DEBUGGER_INFORMATION {
    BOOLEAN DebuggerEnabled;
    BOOLEAN DebuggerNotPresent;
};

bool kernel_debugger_present() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        return false;
    }

    auto* query = reinterpret_cast<NtQuerySystemInformation_t>(
        GetProcAddress(ntdll, "NtQuerySystemInformation"));
    if (!query) {
        return false;
    }

    SYSTEM_KERNEL_DEBUGGER_INFORMATION info{};
    if (query(kSystemKernelDebuggerInformation, &info, sizeof(info), nullptr) < 0) {
        return false;
    }

    return info.DebuggerEnabled && !info.DebuggerNotPresent;
}

bool thread_hidden_from_debugger() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        return false;
    }

    auto* query = reinterpret_cast<NtQueryInformationThread_t>(
        GetProcAddress(ntdll, "NtQueryInformationThread"));
    if (!query) {
        return false;
    }

    ULONG hide = 0;
    if (query(GetCurrentThread(), kThreadHideFromDebugger, &hide, sizeof(hide), nullptr) < 0) {
        return false;
    }

    return hide != 0;
}

bool close_handle_debugger_trap() {
    __try {
        CloseHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0xDEADBEEF)));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
    return false;
}

bool process_debug_flags_cleared() {
    using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        return false;
    }

    auto* query = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (!query) {
        return false;
    }

    ULONG flags = 1;
    if (query(GetCurrentProcess(), 31, &flags, sizeof(flags), nullptr) < 0) {
        return false;
    }

    return flags == 0;
}

} // namespace

namespace anticheat {

void AntiBypassGuard::report_once(const std::string& key, const std::string& detail) {
    if (reported_.count(key) != 0) {
        return;
    }
    reported_.insert(key);
    Encryptic::instance().report_violation({
        ViolationType::BypassDetected,
        ViolationSeverity::Critical,
        "Anti-cheat bypass attempt detected",
        detail,
    });
}

void AntiBypassGuard::scan() {
    if (kernel_debugger_present()) {
        report_once("kernel_dbg", "Kernel debugger active");
    }

    if (thread_hidden_from_debugger()) {
        report_once("hide_thread", "ThreadHideFromDebugger flag set");
    }

    if (close_handle_debugger_trap()) {
        report_once("close_handle", "CloseHandle debugger trap triggered");
    }

    if (process_debug_flags_cleared()) {
        report_once("no_debug_flag", "Process debug flags cleared (NoDebugInherit)");
    }
}

} // namespace anticheat

#else

namespace anticheat {

void AntiBypassGuard::scan() {}

} // namespace anticheat

#endif
