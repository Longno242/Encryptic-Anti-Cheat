#include <anticheat/timing_guard.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace anticheat {

void TimingGuard::reset() {
    LARGE_INTEGER freq{};
    LARGE_INTEGER counter{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);

    qpc_freq_ = freq.QuadPart;
    qpc_base_ = counter.QuadPart;
    tick_base_ = GetTickCount64();
    primed_ = qpc_freq_ > 0;
}

bool TimingGuard::scan(double max_drift_ms) {
    if (!primed_) {
        reset();
        return false;
    }

    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);

    const double qpc_ms = (static_cast<double>(counter.QuadPart - qpc_base_) * 1000.0) /
                          static_cast<double>(qpc_freq_);
    const double tick_ms = static_cast<double>(GetTickCount64() - tick_base_);
    const double drift = qpc_ms > tick_ms ? qpc_ms - tick_ms : tick_ms - qpc_ms;

    if (drift > max_drift_ms) {
        reset();
        return true;
    }

    return false;
}

} // namespace anticheat

#else

namespace anticheat {

void TimingGuard::reset() { primed_ = false; }
bool TimingGuard::scan(double) { return false; }

} // namespace anticheat

#endif
