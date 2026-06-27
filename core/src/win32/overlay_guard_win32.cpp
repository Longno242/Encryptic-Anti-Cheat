#include <anticheat/overlay_guard.hpp>
#include <anticheat/encryptic.hpp>
#include <anticheat/platform.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>

namespace {

struct EnumCtx {
    std::vector<std::string> whitelist;
    bool found{false};
    std::string detail;
};

bool whitelisted(const std::wstring& cls, const std::vector<std::string>& whitelist) {
    char utf8[512]{};
    WideCharToMultiByte(CP_UTF8, 0, cls.c_str(), -1, utf8, sizeof(utf8), nullptr, nullptr);
    const auto lower = anticheat::platform::to_lower(utf8);
    for (const auto& w : whitelist) {
        if (lower.find(anticheat::platform::to_lower(w)) != std::string::npos) {
            return true;
        }
    }
    return false;
}

BOOL CALLBACK enum_windows(HWND hwnd, LPARAM param) {
    auto* ctx = reinterpret_cast<EnumCtx*>(param);
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t class_name[256]{};
    wchar_t title[256]{};
    GetClassNameW(hwnd, class_name, 256);
    GetWindowTextW(hwnd, title, 256);

    if (whitelisted(class_name, ctx->whitelist) || whitelisted(title, ctx->whitelist)) {
        return TRUE;
    }

    static const wchar_t* suspicious_classes[] = {
        L"CheatEngine",
        L"TfrmCheatEngine",
        L"TfrmAutoInject",
        L"TfrmMemoryRecords",
        L"TCETrainer",
        L"TMemoryBrowser",
        L"Overlay",
    };

    const std::wstring cls(class_name);
    for (const auto* s : suspicious_classes) {
        if (cls.find(s) != std::wstring::npos) {
            ctx->found = true;
            char utf8[256]{};
            WideCharToMultiByte(CP_UTF8, 0, class_name, -1, utf8, 256, nullptr, nullptr);
            ctx->detail = utf8;
            return FALSE;
        }
    }

    char title_utf8[256]{};
    WideCharToMultiByte(CP_UTF8, 0, title, -1, title_utf8, 256, nullptr, nullptr);
    const std::string title_lower = anticheat::platform::to_lower(title_utf8);
    static const char* suspicious_titles[] = {
        "cheat engine", "memory view", "pointer scan", "speedhack",
        "extreme injector", "xenos", "trainer", "artmoney",
    };
    for (const auto* s : suspicious_titles) {
        if (title_lower.find(s) != std::string::npos) {
            ctx->found = true;
            ctx->detail = std::string(title_utf8) + " [title]";
            return FALSE;
        }
    }

    return TRUE;
}

} // namespace

namespace anticheat {

void OverlayGuard::configure(const std::vector<std::string>& whitelist) {
    whitelist_ = whitelist;
    if (whitelist_.empty()) {
        whitelist_ = {"Steam", "Discord", "Xbox", "GameBar", "NVIDIA", "GeForce", "CEFOSC"};
    }
}

void OverlayGuard::scan() {
    EnumCtx ctx;
    ctx.whitelist = whitelist_;
    EnumWindows(enum_windows, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.found) {
        Encryptic::instance().report_violation({
            ViolationType::SuspiciousOverlay,
            ViolationSeverity::Medium,
            "Suspicious overlay window detected",
            ctx.detail,
        });
    }
}

} // namespace anticheat

#else

namespace anticheat {

void OverlayGuard::configure(const std::vector<std::string>&) {}
void OverlayGuard::scan() {}

} // namespace anticheat

#endif
