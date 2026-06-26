#include <anticheat/handle_guard.hpp>
#include <anticheat/encryptic.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <vector>

namespace {

using NTSTATUS = LONG;
using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

constexpr ULONG kProcessHandleTracing = 38;

struct PROCESS_HANDLE_TRACING_ENABLE_EX {
    ULONG Flags;
    ULONG TotalSlots;
    struct HandleEntry {
        HANDLE Handle;
        void* CallerAddress;
        ULONG Type;
        ULONG GrantedAccess;
    } Handle[1];
};

bool suspicious_access(ULONG access) {
    constexpr ULONG k_vm_read = 0x0010;
    constexpr ULONG k_vm_write = 0x0020;
    constexpr ULONG k_vm_operation = 0x0008;
    constexpr ULONG k_dup = 0x0040;
    return (access & (k_vm_read | k_vm_write | k_vm_operation | k_dup)) != 0;
}

} // namespace

namespace anticheat {

void HandleGuard::scan() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        return;
    }

    auto* query = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (!query) {
        return;
    }

    ULONG size = 0;
    query(GetCurrentProcess(), kProcessHandleTracing, nullptr, 0, &size);
    if (size == 0) {
        return;
    }

    std::vector<std::uint8_t> buffer(size);
    const NTSTATUS status = query(
        GetCurrentProcess(),
        kProcessHandleTracing,
        buffer.data(),
        static_cast<ULONG>(buffer.size()),
        &size);

    if (status < 0) {
        return;
    }

    const auto* info = reinterpret_cast<const PROCESS_HANDLE_TRACING_ENABLE_EX*>(buffer.data());
    const ULONG count = info->TotalSlots;
    const auto* base = reinterpret_cast<const std::uint8_t*>(info->Handle);

    for (ULONG i = 0; i < count; ++i) {
        const auto* entry = reinterpret_cast<const PROCESS_HANDLE_TRACING_ENABLE_EX::HandleEntry*>(
            base + i * sizeof(PROCESS_HANDLE_TRACING_ENABLE_EX::HandleEntry));

        if (!entry->Handle) {
            continue;
        }

        if (suspicious_access(entry->GrantedAccess)) {
            Encryptic::instance().report_violation({
                ViolationType::ExternalHandle,
                ViolationSeverity::High,
                "Suspicious handle access pattern",
                "access=0x" + std::to_string(entry->GrantedAccess),
            });
        }
    }
}

} // namespace anticheat

#else

namespace anticheat {

void HandleGuard::scan() {}

} // namespace anticheat

#endif
