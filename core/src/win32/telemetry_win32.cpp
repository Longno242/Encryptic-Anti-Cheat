#include <anticheat/telemetry.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>

namespace {

std::string violation_type_name(anticheat::ViolationType type) {
    switch (type) {
    case anticheat::ViolationType::UnknownDll: return "unknown_dll";
    case anticheat::ViolationType::DebuggerDetected: return "debugger";
    case anticheat::ViolationType::ApiHooked: return "api_hook";
    case anticheat::ViolationType::MemoryTampered: return "memory_tamper";
    case anticheat::ViolationType::ModuleNotWhitelisted: return "module_whitelist";
    case anticheat::ViolationType::SuspiciousThread: return "suspicious_thread";
    case anticheat::ViolationType::TimingAnomaly: return "timing_anomaly";
    case anticheat::ViolationType::ExternalHandle: return "external_handle";
    case anticheat::ViolationType::CheatProcessDetected: return "cheat_process";
    case anticheat::ViolationType::VirtualMachineDetected: return "vm";
    case anticheat::ViolationType::SuspiciousOverlay: return "overlay";
    case anticheat::ViolationType::SyntheticInputDetected: return "synthetic_input";
    case anticheat::ViolationType::SelfIntegrityFailed: return "self_integrity";
    case anticheat::ViolationType::GameFileTampered: return "game_file_tamper";
    case anticheat::ViolationType::WatchdogFailure: return "watchdog";
    case anticheat::ViolationType::HeartbeatMissed: return "heartbeat_miss";
    case anticheat::ViolationType::TamperDetected: return "tamper";
    default: return "unknown";
    }
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::uint32_t crc32_bytes(const void* data, std::size_t size) {
    static constexpr std::uint32_t polynomial = 0xEDB88320u;
    std::uint32_t crc = 0xFFFFFFFFu;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (polynomial & mask);
        }
    }
    return ~crc;
}

bool http_post_json(const std::string& url, const std::string& api_key, const std::string& body) {
    std::wstring wide_url(url.begin(), url.end());

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256]{};
    wchar_t path[1024]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 1024;

    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts)) {
        return false;
    }

    HINTERNET session = WinHttpOpen(
        L"EncrypticAntiCheat/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) {
        return false;
    }

    HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(
        connect, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!api_key.empty()) {
        headers += L"X-Encryptic-Key: " + std::wstring(api_key.begin(), api_key.end()) + L"\r\n";
    }

    const BOOL sent = WinHttpSendRequest(
        request, headers.c_str(), static_cast<DWORD>(-1), const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);

    bool ok = false;
    if (sent && WinHttpReceiveResponse(request, nullptr)) {
        ok = true;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

} // namespace

namespace anticheat {

void Telemetry::configure(const TelemetryConfig& config) {
    cfg_ = config;
}

std::string Telemetry::module_hash() const {
    HMODULE mod = GetModuleHandleA("encryptic_core.dll");
    if (!mod) {
        mod = GetModuleHandleA(nullptr);
    }
    if (!mod) {
        return "unknown";
    }

    auto* base = reinterpret_cast<const std::uint8_t*>(mod);
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, base, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) {
        return "unknown";
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos.e_lfanew);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (std::memcmp(section[i].Name, ".text", 5) == 0) {
            const auto* text = base + section[i].VirtualAddress;
            const std::uint32_t crc = crc32_bytes(text, section[i].Misc.VirtualSize);
            std::ostringstream oss;
            oss << std::hex << crc;
            return oss.str();
        }
    }
    return "unknown";
}

void Telemetry::queue_violation(const Violation& violation) {
    if (!cfg_.enabled || cfg_.endpoint_url.empty()) {
        return;
    }

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    std::ostringstream body;
    body << '{'
         << "\"session_id\":\"" << json_escape(cfg_.session_id) << "\","
         << "\"timestamp_ms\":" << now << ","
         << "\"module_hash\":\"" << module_hash() << "\","
         << "\"type\":\"" << violation_type_name(violation.type) << "\","
         << "\"severity\":" << static_cast<int>(violation.severity) << ","
         << "\"message\":\"" << json_escape(violation.message) << "\","
         << "\"detail\":\"" << json_escape(violation.detail) << "\""
         << '}';

    http_post_json(cfg_.endpoint_url, cfg_.api_key, body.str());
    ++pending_;
}

void Telemetry::tick() {
    (void)pending_;
}

} // namespace anticheat

#else

namespace anticheat {

void Telemetry::configure(const TelemetryConfig&) {}
void Telemetry::queue_violation(const Violation&) {}
void Telemetry::tick() {}
std::string Telemetry::module_hash() const { return "unknown"; }

} // namespace anticheat

#endif
