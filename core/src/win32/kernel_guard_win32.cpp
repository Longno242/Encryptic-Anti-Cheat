#include <anticheat/kernel_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#include <encryptic_kernel.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <random>
#include <string>

namespace {

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), len);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !wide[0]) {
        return {};
    }
    char utf8[512]{};
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

void copy_blocked_names(const std::vector<std::string>& names,
                        wchar_t out[][ENCRYPTIC_BLOCKED_NAME_CHARS],
                        uint32_t& count) {
    count = 0;
    for (const auto& name : names) {
        if (count >= ENCRYPTIC_MAX_BLOCKED_NAMES) {
            break;
        }
        const std::wstring wide = utf8_to_wide(name);
        if (wide.empty()) {
            continue;
        }
        wcsncpy_s(out[count], ENCRYPTIC_BLOCKED_NAME_CHARS, wide.c_str(), _TRUNCATE);
        ++count;
    }
}

bool device_ioctl(HANDLE device, DWORD code, void* in, DWORD in_size, void* out, DWORD out_size,
                  DWORD* bytes_returned) {
    return DeviceIoControl(device, code, in, in_size, out, out_size, bytes_returned, nullptr) != FALSE;
}

std::uint64_t random_session_token() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dist(1, UINT64_MAX);
    return dist(gen);
}

} // namespace

namespace anticheat {

void KernelGuard::configure(const KernelGuardSettings& settings,
                            const std::vector<std::string>& blocked_processes,
                            const std::vector<std::string>& blocked_drivers,
                            const std::vector<std::string>& blocked_modules) {
    settings_ = settings;
    blocked_processes_ = blocked_processes;
    blocked_drivers_ = blocked_drivers;
    blocked_modules_ = blocked_modules;
}

bool KernelGuard::connect() {
    if (connected_ && device_handle_) {
        return true;
    }

    const std::wstring device_wide = utf8_to_wide(settings_.device_path.empty()
                                                      ? "\\\\.\\EncrypticGuard"
                                                      : settings_.device_path);
    if (device_wide.empty()) {
        return false;
    }

    HANDLE device = CreateFileW(
        device_wide.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (device == INVALID_HANDLE_VALUE) {
        return false;
    }

    ENCRYPTIC_KERNEL_VER_INFO ver_info{};
    DWORD returned = 0;
    if (!device_ioctl(device, IOCTL_ENCRYPTIC_GET_VERSION, nullptr, 0, &ver_info, sizeof(ver_info),
                      &returned)) {
        CloseHandle(device);
        return false;
    }

    session_token_ = random_session_token();

    ENCRYPTIC_KERNEL_TARGET target{};
    target.process_id = GetCurrentProcessId();
    target.client_pid = GetCurrentProcessId();
    target.session_token = session_token_;
    if (settings_.protect_handles) {
        target.flags |= ENCRYPTIC_TARGET_PROTECT_HANDLES;
    }
    if (settings_.block_drivers) {
        target.flags |= ENCRYPTIC_TARGET_BLOCK_DRIVERS;
    }
    if (settings_.block_modules) {
        target.flags |= ENCRYPTIC_TARGET_BLOCK_MODULES;
    }
    if (settings_.block_global_cheats) {
        target.flags |= ENCRYPTIC_TARGET_BLOCK_GLOBAL_CHEATS;
    }

    if (!device_ioctl(device, IOCTL_ENCRYPTIC_SET_TARGET, &target, sizeof(target), nullptr, 0,
                      &returned)) {
        CloseHandle(device);
        return false;
    }

    device_handle_ = device;
    connected_ = true;
    push_config();
    return true;
}

void KernelGuard::disconnect() {
    if (device_handle_ && session_token_ != 0) {
        ENCRYPTIC_KERNEL_SESSION session{};
        session.session_token = session_token_;
        DWORD returned = 0;
        device_ioctl(static_cast<HANDLE>(device_handle_), IOCTL_ENCRYPTIC_CLEAR_TARGET, &session,
                     sizeof(session), nullptr, 0, &returned);
    }

    if (device_handle_) {
        CloseHandle(static_cast<HANDLE>(device_handle_));
        device_handle_ = nullptr;
    }
    connected_ = false;
    session_token_ = 0;
}

bool KernelGuard::push_config() {
    if (!device_handle_) {
        return false;
    }

    ENCRYPTIC_KERNEL_CONFIG cfg{};
    copy_blocked_names(blocked_processes_, cfg.blocked_processes, cfg.blocked_process_count);
    copy_blocked_names(blocked_drivers_, cfg.blocked_drivers, cfg.blocked_driver_count);
    copy_blocked_names(blocked_modules_, cfg.blocked_modules, cfg.blocked_module_count);

    DWORD returned = 0;
    return device_ioctl(static_cast<HANDLE>(device_handle_), IOCTL_ENCRYPTIC_UPDATE_CONFIG, &cfg,
                        sizeof(cfg), nullptr, 0, &returned);
}

void KernelGuard::report_once(const std::string& key, ViolationType type, const std::string& message,
                              const std::string& detail) {
    if (reported_.count(key) != 0) {
        return;
    }
    reported_.insert(key);
    Encryptic::instance().report_violation({
        type,
        ViolationSeverity::Critical,
        message,
        detail,
    });
}

void KernelGuard::handle_event(const ENCRYPTIC_KERNEL_EVENT& ev) {
    const std::string detail = wide_to_utf8(ev.detail);
    const std::string key = std::to_string(ev.type) + ":" + detail + ":" + std::to_string(ev.source_pid);

    switch (static_cast<ENCRYPTIC_KERNEL_EVENT_TYPE>(ev.type)) {
    case EncrypticKernelBlockedProcess:
        report_once(key, ViolationType::CheatProcessDetected, "Kernel blocked cheat process launch",
                    detail);
        break;
    case EncrypticKernelSuspiciousImage:
        report_once(key, ViolationType::InjectionDetected, "Kernel blocked suspicious module load",
                    detail);
        break;
    case EncrypticKernelHandleStripped:
        report_once(key, ViolationType::ExternalHandle, "Kernel blocked external memory access",
                    detail.empty() ? "source_pid=" + std::to_string(ev.source_pid) : detail);
        break;
    case EncrypticKernelBlockedDriver:
        report_once(key, ViolationType::ThreatToolDetected, "Kernel blocked cheat driver in game", detail);
        break;
    case EncrypticKernelSessionStarted:
    case EncrypticKernelSessionCleared:
    case EncrypticKernelProtectedProcessExit:
        break;
    default:
        break;
    }
}

void KernelGuard::scan() {
    if (!settings_.enabled) {
        return;
    }

    if (!connected_) {
        if (connect()) {
            warned_no_driver_ = false;
        } else if (!warned_no_driver_) {
            warned_no_driver_ = true;
            Encryptic::instance().report_violation({
                ViolationType::KernelDriverUnavailable,
                ViolationSeverity::Low,
                "Kernel driver not loaded",
                "Install EncrypticGuard.sys — scoped mode avoids affecting other games/anti-cheat",
            });
        }
        return;
    }

    ENCRYPTIC_KERNEL_EVENT_BATCH batch{};
    DWORD returned = 0;
    if (!device_ioctl(static_cast<HANDLE>(device_handle_), IOCTL_ENCRYPTIC_POLL_EVENTS, nullptr, 0,
                      &batch, sizeof(batch), &returned)) {
        disconnect();
        return;
    }

    for (uint32_t i = 0; i < batch.count && i < ENCRYPTIC_MAX_EVENTS_PER_POLL; ++i) {
        handle_event(batch.events[i]);
    }
}

} // namespace anticheat

#else

namespace anticheat {

void KernelGuard::configure(const KernelGuardSettings&, const std::vector<std::string>&,
                            const std::vector<std::string>&, const std::vector<std::string>&) {}
bool KernelGuard::connect() { return false; }
void KernelGuard::disconnect() {}
bool KernelGuard::push_config() { return false; }
void KernelGuard::scan() {}

} // namespace anticheat

#endif
