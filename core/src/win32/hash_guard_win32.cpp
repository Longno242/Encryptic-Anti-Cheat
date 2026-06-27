#include <anticheat/hash_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <string>

namespace {

std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !wide[0]) {
        return {};
    }
    char utf8[MAX_PATH * 2]{};
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

bool get_process_path(DWORD pid, std::string& out) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return false;
    }
    wchar_t path[MAX_PATH * 2]{};
    DWORD size = static_cast<DWORD>(std::size(path));
    const BOOL ok = QueryFullProcessImageNameW(proc, 0, path, &size);
    CloseHandle(proc);
    if (!ok) {
        return false;
    }
    out = wide_to_utf8(path);
    return !out.empty();
}

} // namespace

namespace anticheat {

void HashGuard::configure(const std::vector<CheatHashEntry>& known_hashes) {
    known_ = known_hashes;
    for (auto& e : known_) {
        e.sha256_hex = platform::to_lower(e.sha256_hex);
    }
}

void HashGuard::report_once(const std::string& key, const std::string& detail) {
    if (reported_.count(key) != 0) {
        return;
    }
    reported_.insert(key);
    Encryptic::instance().report_violation({
        ViolationType::KnownCheatHash,
        ViolationSeverity::Critical,
        "Known cheat binary hash matched",
        detail,
    });
}

void HashGuard::scan() {
    if (known_.empty()) {
        return;
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID <= 4) {
                continue;
            }

            std::string path;
            if (!get_process_path(entry.th32ProcessID, path)) {
                continue;
            }

            const std::string hash = platform::sha256_file(path);
            if (hash.empty()) {
                continue;
            }

            const std::string hash_lower = platform::to_lower(hash);
            for (const auto& known : known_) {
                if (hash_lower == known.sha256_hex) {
                    report_once(known.sha256_hex, known.label + " — " + path);
                    break;
                }
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
}

} // namespace anticheat

#else

namespace anticheat {

void HashGuard::configure(const std::vector<CheatHashEntry>&) {}
void HashGuard::scan() {}

} // namespace anticheat

#endif
