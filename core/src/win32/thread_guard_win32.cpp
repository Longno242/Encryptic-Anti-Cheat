#include <anticheat/thread_guard.hpp>
#include <anticheat/encryptic.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace anticheat {

void ThreadGuard::establish_baseline() {
    baseline_count_ = 0;
    const DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snap, &entry)) {
        do {
            if (entry.th32OwnerProcessID == pid) {
                ++baseline_count_;
            }
        } while (Thread32Next(snap, &entry));
    }

    CloseHandle(snap);
}

void ThreadGuard::scan() {
    const DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }

    unsigned count = 0;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    if (Thread32First(snap, &entry)) {
        do {
            if (entry.th32OwnerProcessID != pid) {
                continue;
            }
            ++count;

            HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
            if (!thread) {
                continue;
            }

            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(thread, &ctx)) {
#if defined(_M_X64) || defined(__x86_64__)
                const void* start = reinterpret_cast<const void*>(ctx.Rcx);
#else
                const void* start = reinterpret_cast<const void*>(ctx.Eax);
#endif
                MEMORY_BASIC_INFORMATION mbi{};
                if (start && VirtualQuery(start, &mbi, sizeof(mbi))) {
                    const bool private_exec =
                        mbi.Type == MEM_PRIVATE &&
                        (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

                    if (private_exec && mbi.AllocationBase == mbi.BaseAddress) {
                        Encryptic::instance().report_violation({
                            ViolationType::SuspiciousThread,
                            ViolationSeverity::High,
                            "Thread with private executable start address",
                            "tid=" + std::to_string(entry.th32ThreadID),
                        });
                    }
                }
            }

            CloseHandle(thread);
        } while (Thread32Next(snap, &entry));
    }

    CloseHandle(snap);

    if (baseline_count_ > 0 && count > baseline_count_ + 20) {
        Encryptic::instance().report_violation({
            ViolationType::SuspiciousThread,
            ViolationSeverity::Medium,
            "Unexpected thread count growth",
            "count=" + std::to_string(count),
        });
    }
}

} // namespace anticheat

#else

namespace anticheat {

void ThreadGuard::establish_baseline() {}
void ThreadGuard::scan() {}

} // namespace anticheat

#endif
