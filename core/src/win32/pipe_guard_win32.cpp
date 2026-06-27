#include <anticheat/pipe_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace {

bool contains_icase(const std::string& haystack, const std::string& needle) {
    return anticheat::platform::to_lower(haystack).find(anticheat::platform::to_lower(needle)) !=
           std::string::npos;
}

std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !wide[0]) {
        return {};
    }
    char utf8[512]{};
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

} // namespace

namespace anticheat {

void PipeGuard::configure(const std::vector<std::string>& blocked_pipes,
                          const std::vector<std::string>& blocked_mutexes) {
    blocked_pipes_ = blocked_pipes;
    if (blocked_pipes_.empty()) {
        blocked_pipes_ = {
            "cedriver", "dbk", "cheatengine", "ce_", "ceserver", "dbvm", "blackbone",
            "xenos", "extremeinjector", "processhacker",
        };
    }

    blocked_mutexes_ = blocked_mutexes;
    if (blocked_mutexes_.empty()) {
        blocked_mutexes_ = {
            "DBVM", "CEDRIVER", "DBK", "CE_", "DBK64", "DBK32",
        };
    }
}

void PipeGuard::report_once(const std::string& key, const std::string& message,
                            const std::string& detail) {
    if (reported_.count(key) != 0) {
        return;
    }
    reported_.insert(key);
    Encryptic::instance().report_violation({
        ViolationType::SuspiciousPipe,
        ViolationSeverity::High,
        message,
        detail,
    });
}

void PipeGuard::scan() {
    WIN32_FIND_DATAW fd{};
    HANDLE find = FindFirstFileW(L"\\\\.\\pipe\\*", &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            const std::string name = wide_to_utf8(fd.cFileName);
            for (const auto& blocked : blocked_pipes_) {
                if (contains_icase(name, blocked)) {
                    report_once("pipe:" + name, "Suspicious named pipe detected", name);
                    break;
                }
            }
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }

    for (const auto& mutex_name : blocked_mutexes_) {
        const std::wstring wide(mutex_name.begin(), mutex_name.end());
        HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, wide.c_str());
        if (mutex) {
            CloseHandle(mutex);
            report_once("mutex:" + mutex_name, "Cheat tool mutex/object detected", mutex_name);
        }
    }
}

} // namespace anticheat

#else

namespace anticheat {

void PipeGuard::configure(const std::vector<std::string>&, const std::vector<std::string>&) {}
void PipeGuard::scan() {}

} // namespace anticheat

#endif
