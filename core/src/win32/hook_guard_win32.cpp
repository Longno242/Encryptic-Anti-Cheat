#include <anticheat/hook_guard.hpp>
#include <anticheat/encryptic.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>

namespace anticheat {

void HookGuard::sample_api(const char* module, const char* export_name, ApiSample& out) {
    out.module = module;
    out.export_name = export_name;
    out.address = nullptr;
    std::memset(out.bytes, 0, sizeof(out.bytes));

    HMODULE mod = GetModuleHandleA(module);
    if (!mod) {
        mod = LoadLibraryA(module);
    }
    if (!mod) {
        return;
    }

    out.address = reinterpret_cast<void*>(GetProcAddress(mod, export_name));
    if (!out.address) {
        return;
    }

    DWORD old = 0;
    if (VirtualProtect(out.address, sizeof(out.bytes), PAGE_EXECUTE_READ, &old)) {
        std::memcpy(out.bytes, out.address, sizeof(out.bytes));
        VirtualProtect(out.address, sizeof(out.bytes), old, &old);
    }
}

bool HookGuard::is_hooked(const ApiSample& original, void* current) const {
    if (!original.address || !current) {
        return false;
    }

    if (original.address != current) {
        return true;
    }

    std::uint8_t live[16]{};
    DWORD old = 0;
    if (!VirtualProtect(current, sizeof(live), PAGE_EXECUTE_READ, &old)) {
        return false;
    }
    std::memcpy(live, current, sizeof(live));
    VirtualProtect(current, sizeof(live), old, &old);

    if (std::memcmp(original.bytes, live, sizeof(live)) == 0) {
        return false;
    }

    const auto b0 = live[0];
    if (b0 == 0xE9 || b0 == 0xEB) {
        return true;
    }
#if defined(_M_X64) || defined(__x86_64__)
    if (b0 == 0x48 && live[1] == 0xB8 && live[10] == 0xFF && live[11] == 0xE0) {
        return true;
    }
#endif

    return true;
}

void HookGuard::prime() {
    const char* apis[][2] = {
        {"kernel32.dll", "LoadLibraryW"},
        {"kernel32.dll", "LoadLibraryA"},
        {"kernel32.dll", "VirtualProtect"},
        {"kernel32.dll", "WriteProcessMemory"},
        {"kernel32.dll", "CreateRemoteThread"},
        {"ntdll.dll", "NtWriteVirtualMemory"},
        {"ntdll.dll", "NtProtectVirtualMemory"},
    };

    for (const auto& api : apis) {
        ApiSample sample;
        sample_api(api[0], api[1], sample);
        if (sample.address) {
            const std::string key = std::string(api[0]) + "!" + api[1];
            baselines_[key] = sample;
        }
    }
}

void HookGuard::scan() {
    for (const auto& [key, baseline] : baselines_) {
        HMODULE mod = GetModuleHandleA(baseline.module.c_str());
        if (!mod) {
            continue;
        }

        void* current = reinterpret_cast<void*>(GetProcAddress(mod, baseline.export_name.c_str()));
        if (is_hooked(baseline, current)) {
            Encryptic::instance().report_violation({
                ViolationType::ApiHooked,
                ViolationSeverity::Critical,
                "API hook detected",
                key,
            });
        }
    }
}

} // namespace anticheat

#else

namespace anticheat {

void HookGuard::prime() {}
void HookGuard::scan() {}

} // namespace anticheat

#endif
