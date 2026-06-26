#include <anticheat/dll_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

namespace {

using LoadLibraryW_t = HMODULE(WINAPI*)(LPCWSTR);

LoadLibraryW_t g_original_load_library_w = nullptr;

HMODULE WINAPI hooked_load_library_w(LPCWSTR name) {
    if (name != nullptr) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string utf8(static_cast<std::size_t>(len), '\0');
            WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8.data(), len, nullptr, nullptr);
            if (!utf8.empty() && utf8.back() == '\0') {
                utf8.pop_back();
            }

            anticheat::Encryptic::instance().report_violation({
                anticheat::ViolationType::UnknownDll,
                anticheat::ViolationSeverity::High,
                "LoadLibraryW intercepted",
                utf8,
            });
        }
    }

    return g_original_load_library_w ? g_original_load_library_w(name) : nullptr;
}

bool write_jmp(void* target, void* detour) {
    DWORD old_protect = 0;
    if (!VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

#if defined(_M_X64) || defined(__x86_64__)
    auto* patch = static_cast<std::uint8_t*>(target);
    patch[0] = 0x48;
    patch[1] = 0xB8;
    *reinterpret_cast<void**>(patch + 2) = detour;
    patch[10] = 0xFF;
    patch[11] = 0xE0;
#else
    auto* patch = static_cast<std::uint8_t*>(target);
    patch[0] = 0xE9;
    const auto rel = reinterpret_cast<std::uintptr_t>(detour) -
                     reinterpret_cast<std::uintptr_t>(target) - 5;
    *reinterpret_cast<std::uint32_t*>(patch + 1) = static_cast<std::uint32_t>(rel);
#endif

    VirtualProtect(target, 14, old_protect, &old_protect);
    FlushInstructionCache(GetCurrentProcess(), target, 14);
    return true;
}

} // namespace

namespace anticheat {

void DllGuard::start(bool block_unknown, const std::vector<std::string>& allowed) {
    block_unknown_ = block_unknown;
    allowed_.clear();
    for (const auto& m : allowed) {
        allowed_.insert(platform::to_lower(platform::basename(m)));
    }

    for (const auto& mod : snapshot_modules()) {
        baseline_.insert(platform::to_lower(mod.name));
    }
}

bool DllGuard::is_allowed(const std::string& name) const {
    if (allowed_.empty()) {
        return true;
    }
    return allowed_.count(platform::to_lower(name)) > 0;
}

std::vector<LoadedModule> DllGuard::snapshot_modules() const {
    std::vector<LoadedModule> modules;
    HMODULE handles[1024]{};
    DWORD needed = 0;

    if (!EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles), &needed)) {
        return modules;
    }

    const unsigned count = needed / sizeof(HMODULE);
    for (unsigned i = 0; i < count; ++i) {
        char path[MAX_PATH]{};
        if (GetModuleFileNameA(handles[i], path, MAX_PATH) == 0) {
            continue;
        }

        MODULEINFO info{};
        GetModuleInformation(GetCurrentProcess(), handles[i], &info, sizeof(info));

        LoadedModule mod;
        mod.path = path;
        mod.name = platform::basename(mod.path);
        mod.base = info.lpBaseOfDll;
        mod.size = info.SizeOfImage;
        mod.signed_module = platform::verify_authenticode(mod.path);
        modules.push_back(std::move(mod));
    }

    return modules;
}

void DllGuard::scan() {
    for (const auto& mod : snapshot_modules()) {
        const auto key = platform::to_lower(mod.name);
        if (baseline_.count(key) > 0) {
            continue;
        }

        baseline_.insert(key);

        if (!is_allowed(mod.name)) {
            Encryptic::instance().report_violation({
                ViolationType::UnknownDll,
                ViolationSeverity::Critical,
                "Unauthorized module loaded",
                mod.path,
            });
        } else if (!mod.signed_module && Encryptic::instance().config().require_signature) {
            Encryptic::instance().report_violation({
                ViolationType::UnknownDll,
                ViolationSeverity::High,
                "Unsigned module loaded",
                mod.path,
            });
        } else {
            Encryptic::instance().report_violation({
                ViolationType::UnknownDll,
                ViolationSeverity::Medium,
                "New module detected",
                mod.path,
            });
        }
    }
}

bool DllGuard::install_load_library_hook() {
    if (hook_installed_) {
        return true;
    }

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return false;
    }

    auto* target = reinterpret_cast<void*>(GetProcAddress(kernel32, "LoadLibraryW"));
    if (!target) {
        return false;
    }

    g_original_load_library_w = reinterpret_cast<LoadLibraryW_t>(target);
    if (!write_jmp(target, reinterpret_cast<void*>(&hooked_load_library_w))) {
        return false;
    }

    hook_installed_ = true;
    return true;
}

void DllGuard::remove_load_library_hook() {
    hook_installed_ = false;
}

} // namespace anticheat

#else

namespace anticheat {

void DllGuard::start(bool, const std::vector<std::string>&) {}
void DllGuard::scan() {}
bool DllGuard::install_load_library_hook() { return false; }
void DllGuard::remove_load_library_hook() {}

} // namespace anticheat

#endif
