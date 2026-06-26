#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace anticheat {

enum class ViolationType : std::uint8_t {
    UnknownDll,
    DebuggerDetected,
    ApiHooked,
    MemoryTampered,
    ModuleNotWhitelisted,
    SuspiciousThread,
    TimingAnomaly,
    ExternalHandle,
    CheatProcessDetected,
    VirtualMachineDetected,
    SuspiciousOverlay,
    SyntheticInputDetected,
    SelfIntegrityFailed,
    GameFileTampered,
    WatchdogFailure,
    HeartbeatMissed,
    TamperDetected,
};

enum class ViolationSeverity : std::uint8_t {
    Low,
    Medium,
    High,
    Critical,
};

enum class ProtectionPreset : std::uint8_t {
    Minimal,
    Balanced,
    Aggressive,
    Custom,
};

struct Violation {
    ViolationType type{};
    ViolationSeverity severity{ViolationSeverity::Medium};
    std::string message;
    std::string detail;
};

using ViolationCallback = std::function<void(const Violation&)>;

struct MemoryRegion {
    const void* address{};
    std::size_t size{};
    std::string tag;
};

struct FileHashEntry {
    std::string path;
    std::string sha256_hex;
};

} // namespace anticheat
