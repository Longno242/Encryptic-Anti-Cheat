#include <anticheat/heartbeat_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <chrono>
#include <sstream>

namespace {

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

bool http_post(const std::string& url, const std::string& body) {
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

    HINTERNET session = WinHttpOpen(L"Encryptic/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
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

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    const BOOL ok = WinHttpSendRequest(
        request, headers, static_cast<DWORD>(-1), const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
                    WinHttpReceiveResponse(request, nullptr);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok != FALSE;
}

} // namespace

namespace anticheat {

void HeartbeatGuard::configure(const HeartbeatConfig& config) {
    cfg_ = config;
    last_send_ms_ = now_ms();
    misses_ = 0;
}

void HeartbeatGuard::tick() {
    if (!cfg_.enabled || cfg_.endpoint_url.empty()) {
        return;
    }

    const std::uint64_t now = now_ms();
    if (now - last_send_ms_ < cfg_.interval_ms) {
        return;
    }

    std::ostringstream body;
    body << "{\"session_token\":\"" << cfg_.session_token << "\",\"timestamp_ms\":" << now;
    if (!cfg_.hmac_secret.empty()) {
        const std::string payload = cfg_.session_token + ":" + std::to_string(now);
        const std::string sig = platform::hmac_sha256_hex(cfg_.hmac_secret, payload);
        body << ",\"signature\":\"" << sig << "\"";
    }
    body << "}";

    if (http_post(cfg_.endpoint_url, body.str())) {
        misses_ = 0;
    } else {
        ++misses_;
        if (misses_ >= cfg_.miss_threshold) {
            Encryptic::instance().report_violation({
                ViolationType::HeartbeatMissed,
                ViolationSeverity::Critical,
                "Heartbeat missed",
                "Failed server heartbeat threshold",
            });
            misses_ = 0;
        }
    }

    last_send_ms_ = now;
}

bool HeartbeatGuard::missed_threshold() const {
    return misses_ >= cfg_.miss_threshold;
}

} // namespace anticheat

#else

namespace anticheat {

void HeartbeatGuard::configure(const HeartbeatConfig&) {}
void HeartbeatGuard::tick() {}
bool HeartbeatGuard::missed_threshold() const { return false; }

} // namespace anticheat

#endif
