#include <anticheat/watchdog_guard.hpp>
#include <anticheat/encryptic.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace {

std::atomic<anticheat::WatchdogGuard*> g_watchdog{nullptr};
std::atomic<bool> g_watchdog_thread_run{false};
std::thread g_watchdog_thread;

void watchdog_thread_main() {
    while (g_watchdog_thread_run.load()) {
        if (auto* wd = g_watchdog.load()) {
            if (!wd->scan()) {
                anticheat::Encryptic::instance().report_violation({
                    anticheat::ViolationType::WatchdogFailure,
                    anticheat::ViolationSeverity::Critical,
                    "Watchdog failure",
                    "Anti-tamper watchdog detected stall or canary mismatch",
                });
            }
        }
        Sleep(500);
    }
}

} // namespace

namespace anticheat {

void WatchdogGuard::start(std::uint32_t interval_ms) {
    interval_ms_ = interval_ms;
    canary_ = 0xEAC12345u ^ static_cast<std::uint32_t>(GetTickCount());
    running_ = true;
    last_ping_ms_ = static_cast<std::uint64_t>(GetTickCount64());
    tick_count_ = 0;

    g_watchdog.store(this);
    if (!g_watchdog_thread_run.exchange(true)) {
        g_watchdog_thread = std::thread(watchdog_thread_main);
    }
}

void WatchdogGuard::stop() {
    running_ = false;
    g_watchdog.store(nullptr);
}

void WatchdogGuard::ping() {
    ++tick_count_;
    last_ping_ms_ = static_cast<std::uint64_t>(GetTickCount64());
    std::uint32_t c = canary_.load();
    c ^= 0xA5A5A5A5u;
    c = (c << 3) | (c >> 29);
    canary_.store(c);
}

bool WatchdogGuard::scan() {
    if (!running_.load()) {
        return true;
    }

    const std::uint64_t now = static_cast<std::uint64_t>(GetTickCount64());
    if (now - last_ping_ms_.load() > interval_ms_ * 3ULL) {
        return false;
    }

    const std::uint32_t c = canary_.load();
    if (c == 0 || c == 0xEAC12345u) {
        return false;
    }

    return true;
}

} // namespace anticheat

#else

namespace anticheat {

void WatchdogGuard::start(std::uint32_t) {}
void WatchdogGuard::stop() {}
void WatchdogGuard::ping() {}
bool WatchdogGuard::scan() { return true; }

} // namespace anticheat

#endif
