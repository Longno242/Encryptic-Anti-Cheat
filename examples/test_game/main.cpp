#include <anticheat/anticheat.hpp>
#include "kick_dialog.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <atomic>
#include <deque>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr UINT WM_ENCRYPTIC_KICK = WM_APP + 2;

struct GameState {
    float player_x = 400.0f;
    float player_y = 300.0f;
    std::int32_t health = 100;
    std::int32_t score = 0;
};

struct ViolationEntry {
    std::string severity;
    std::string type;
    std::string message;
    std::string detail;
    std::string time;
};

HWND g_hwnd = nullptr;
HINSTANCE g_instance = nullptr;
std::atomic<bool> g_kick_triggered{false};
std::chrono::steady_clock::time_point g_anticheat_started{};
GameState g_game{};
bool g_keys[256]{};
std::deque<ViolationEntry> g_violations;
std::mutex g_violation_mutex;
int g_violation_scroll = 0;
bool g_hook_installed = false;
std::uint8_t g_hook_original[16]{};
void* g_hook_target = nullptr;
bool g_timing_cheat_pending = false;

static HANDLE WINAPI hooked_create_remote_thread(
    HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD) {
    return nullptr;
}

bool install_jmp_hook(void* target, void* detour, std::uint8_t* original_out) {
    DWORD old = 0;
    if (!VirtualProtect(target, sizeof(g_hook_original), PAGE_EXECUTE_READWRITE, &old)) {
        return false;
    }

    std::memcpy(original_out, target, sizeof(g_hook_original));

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

    VirtualProtect(target, sizeof(g_hook_original), old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(g_hook_original));
    return true;
}

const wchar_t* kWindowClass = L"EncrypticTestGame";

void draw_text(HDC hdc, int x, int y, COLORREF color, const std::wstring& text);

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

std::string now_time_string() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[32]{};
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

const char* violation_type_name(anticheat::ViolationType type) {
    switch (type) {
    case anticheat::ViolationType::UnknownDll: return "UnknownDll";
    case anticheat::ViolationType::DebuggerDetected: return "DebuggerDetected";
    case anticheat::ViolationType::ApiHooked: return "ApiHooked";
    case anticheat::ViolationType::MemoryTampered: return "MemoryTampered";
    case anticheat::ViolationType::ModuleNotWhitelisted: return "ModuleNotWhitelisted";
    case anticheat::ViolationType::SuspiciousThread: return "SuspiciousThread";
    case anticheat::ViolationType::TimingAnomaly: return "TimingAnomaly";
    case anticheat::ViolationType::ExternalHandle: return "ExternalHandle";
    case anticheat::ViolationType::CheatProcessDetected: return "CheatProcessDetected";
    case anticheat::ViolationType::VirtualMachineDetected: return "VirtualMachineDetected";
    case anticheat::ViolationType::SuspiciousOverlay: return "SuspiciousOverlay";
    case anticheat::ViolationType::SyntheticInputDetected: return "SyntheticInputDetected";
    case anticheat::ViolationType::SelfIntegrityFailed: return "SelfIntegrityFailed";
    case anticheat::ViolationType::GameFileTampered: return "GameFileTampered";
    case anticheat::ViolationType::WatchdogFailure: return "WatchdogFailure";
    case anticheat::ViolationType::HeartbeatMissed: return "HeartbeatMissed";
    case anticheat::ViolationType::TamperDetected: return "TamperDetected";
    default: return "Unknown";
    }
}

const char* severity_name(anticheat::ViolationSeverity sev) {
    switch (sev) {
    case anticheat::ViolationSeverity::Low: return "LOW";
    case anticheat::ViolationSeverity::Medium: return "MED";
    case anticheat::ViolationSeverity::High: return "HIGH";
    case anticheat::ViolationSeverity::Critical: return "CRIT";
    default: return "?";
    }
}

COLORREF severity_color(anticheat::ViolationSeverity sev) {
    switch (sev) {
    case anticheat::ViolationSeverity::Low: return RGB(140, 200, 140);
    case anticheat::ViolationSeverity::Medium: return RGB(220, 200, 80);
    case anticheat::ViolationSeverity::High: return RGB(255, 140, 60);
    case anticheat::ViolationSeverity::Critical: return RGB(255, 70, 70);
    default: return RGB(200, 200, 200);
    }
}

void push_violation_ui(const anticheat::Violation& v) {
    ViolationEntry entry;
    entry.severity = severity_name(v.severity);
    entry.type = violation_type_name(v.type);
    entry.message = v.message;
    entry.detail = v.detail;
    entry.time = now_time_string();

    {
        std::lock_guard lock(g_violation_mutex);
        g_violations.push_front(entry);
        while (g_violations.size() > 80) {
            g_violations.pop_back();
        }
    }

    if (g_hwnd) {
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}

std::string to_lower_ascii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool is_cheat_api_hook(const anticheat::Violation& v) {
    if (v.type != anticheat::ViolationType::ApiHooked) {
        return false;
    }
    const std::string detail = to_lower_ascii(v.detail);
    if (detail.find("loadlibraryw") != std::string::npos ||
        detail.find("loadlibrarya") != std::string::npos) {
        return false;
    }
    return true;
}

bool should_kick_on_violation(const anticheat::Violation& v) {
    const std::string detail = to_lower_ascii(v.detail);

    const auto elapsed =
        std::chrono::steady_clock::now() - g_anticheat_started;
    const bool warmed_up = elapsed >= std::chrono::seconds(8);

    switch (v.type) {
    case anticheat::ViolationType::MemoryTampered:
    case anticheat::ViolationType::TimingAnomaly:
    case anticheat::ViolationType::SyntheticInputDetected:
        return true;

    case anticheat::ViolationType::ApiHooked:
        return is_cheat_api_hook(v);

    case anticheat::ViolationType::UnknownDll:
    case anticheat::ViolationType::ModuleNotWhitelisted:
        return detail.find("evil.dll") != std::string::npos;

    case anticheat::ViolationType::SuspiciousThread:
        return v.message.find("private executable") != std::string::npos;

    case anticheat::ViolationType::DebuggerDetected:
    case anticheat::ViolationType::CheatProcessDetected:
        return warmed_up;

    case anticheat::ViolationType::ExternalHandle:
    case anticheat::ViolationType::VirtualMachineDetected:
    case anticheat::ViolationType::SuspiciousOverlay:
    case anticheat::ViolationType::HeartbeatMissed:
    case anticheat::ViolationType::WatchdogFailure:
    case anticheat::ViolationType::SelfIntegrityFailed:
    case anticheat::ViolationType::GameFileTampered:
    case anticheat::ViolationType::TamperDetected:
    default:
        return false;
    }
}

void trigger_kick(const anticheat::Violation& v) {
    bool expected = false;
    if (!g_kick_triggered.compare_exchange_strong(expected, true)) {
        return;
    }

    push_violation_ui(v);

    std::ostringstream oss;
    oss << "[Encryptic][KICK][" << severity_name(v.severity) << "][" << violation_type_name(v.type) << "] "
        << v.message;
    if (!v.detail.empty()) {
        oss << " — " << v.detail;
    }
    oss << '\n';
    OutputDebugStringA(oss.str().c_str());

    if (g_hwnd) {
        auto* copy = new anticheat::Violation(v);
        PostMessageW(g_hwnd, WM_ENCRYPTIC_KICK, 0, reinterpret_cast<LPARAM>(copy));
    } else {
        anticheat::Encryptic::instance().stop();
        show_kick_dialog(v, g_instance);
    }
}

void on_violation(const anticheat::Violation& v) {
    if (!should_kick_on_violation(v)) {
        push_violation_ui(v);
        return;
    }

    trigger_kick(v);
}

std::wstring config_path_next_to_exe() {
    wchar_t exe_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::wstring path = exe_path;
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    }
    path += L"encryptic_config.json";
    return path;
}

void start_anticheat() {
    anticheat::Config cfg;
    char config_utf8[MAX_PATH * 4]{};
    const std::wstring config_path = config_path_next_to_exe();
    WideCharToMultiByte(CP_UTF8, 0, config_path.c_str(), -1, config_utf8, sizeof(config_utf8), nullptr, nullptr);

    if (!anticheat::load_config_from_file(config_utf8, cfg)) {
        cfg = anticheat::Config::from_preset(anticheat::ProtectionPreset::Aggressive);
    }

    cfg.preset = anticheat::ProtectionPreset::Custom;
    cfg.watermark.game_title = "Encryptic Test Game";
    cfg.watermark.blocking = false;
    cfg.watermark.display_duration_ms = 2800;
    cfg.kick_on_critical = false;
    cfg.require_signature = false;
    cfg.block_unknown_dlls = false;
    cfg.module_whitelist = false;
    cfg.vm_guard = false;
    cfg.handle_guard = false;
    cfg.overlay_guard = false;
    cfg.heartbeat.enabled = false;
    cfg.on_violation = on_violation;

    cfg.hook_scan_interval_ms = 1500;
    cfg.memory_scan_interval_ms = 1000;
    cfg.thread_scan_interval_ms = 1500;
    cfg.timing_check_interval_ms = 500;
    cfg.process_scan_interval_ms = 2000;
    cfg.input_scan_interval_ms = 1000;

    anticheat::Encryptic::instance().start(cfg);
    g_anticheat_started = std::chrono::steady_clock::now();

    anticheat::Encryptic::instance().register_memory_region({
        &g_game.health,
        sizeof(g_game.health),
        "player_health",
    });
    anticheat::Encryptic::instance().register_memory_region({
        &g_game.score,
        sizeof(g_game.score),
        "player_score",
    });
}

void cheat_memory_health() {
    g_game.health = 9999;
}

void cheat_api_hook() {
    if (g_hook_installed) {
        return;
    }

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return;
    }

    // CreateRemoteThread is scanned by HookGuard but not used by the game loop.
    g_hook_target = reinterpret_cast<void*>(GetProcAddress(kernel32, "CreateRemoteThread"));
    if (!g_hook_target) {
        return;
    }

    if (!install_jmp_hook(g_hook_target, reinterpret_cast<void*>(&hooked_create_remote_thread),
                          g_hook_original)) {
        g_hook_target = nullptr;
        return;
    }

    g_hook_installed = true;
}

void restore_api_hook() {
    if (!g_hook_installed || !g_hook_target) {
        return;
    }

    DWORD old = 0;
    if (VirtualProtect(g_hook_target, sizeof(g_hook_original), PAGE_EXECUTE_READWRITE, &old)) {
        std::memcpy(g_hook_target, g_hook_original, sizeof(g_hook_original));
        VirtualProtect(g_hook_target, sizeof(g_hook_original), old, &old);
        FlushInstructionCache(GetCurrentProcess(), g_hook_target, sizeof(g_hook_original));
    }
    g_hook_installed = false;
    g_hook_target = nullptr;
}

void cheat_load_evil_dll() {
    wchar_t exe_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::wstring dll_path = exe_path;
    const auto slash = dll_path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        dll_path.resize(slash + 1);
    }
    dll_path += L"evil.dll";
    LoadLibraryW(dll_path.c_str());
}

void cheat_timing_freeze() {
    g_timing_cheat_pending = true;
}

void cheat_suspicious_thread() {
    void* mem = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        return;
    }

    auto* code = static_cast<std::uint8_t*>(mem);
    code[0] = 0xC3;

    HANDLE thread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(mem), nullptr, 0, nullptr);
    if (thread) {
        WaitForSingleObject(thread, 2000);
        CloseHandle(thread);
    }
}

void cheat_synthetic_input() {
    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 'J';
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'J';
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void update_game(float dt) {
    const float speed = 220.0f;
    if (g_keys['W'] || g_keys[VK_UP]) {
        g_game.player_y -= speed * dt;
    }
    if (g_keys['S'] || g_keys[VK_DOWN]) {
        g_game.player_y += speed * dt;
    }
    if (g_keys['A'] || g_keys[VK_LEFT]) {
        g_game.player_x -= speed * dt;
    }
    if (g_keys['D'] || g_keys[VK_RIGHT]) {
        g_game.player_x += speed * dt;
    }

    g_game.player_x = std::clamp(g_game.player_x, 40.0f, 700.0f);
    g_game.player_y = std::clamp(g_game.player_y, 80.0f, 520.0f);

    if (g_keys[VK_SPACE]) {
        g_game.score += static_cast<std::int32_t>(dt * 10.0f);
    }
}

void draw_text(HDC hdc, int x, int y, COLORREF color, const std::wstring& text) {
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
}

void paint(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int panel_x = width * 58 / 100;

    HBRUSH bg = CreateSolidBrush(RGB(18, 20, 28));
    FillRect(hdc, &client, bg);
    DeleteObject(bg);

    RECT game_rect{0, 0, panel_x, client.bottom};
    HBRUSH game_bg = CreateSolidBrush(RGB(28, 34, 48));
    FillRect(hdc, &game_rect, game_bg);
    DeleteObject(game_bg);

    RECT panel_rect{panel_x, 0, client.right, client.bottom};
    HBRUSH panel_bg = CreateSolidBrush(RGB(14, 16, 22));
    FillRect(hdc, &panel_rect, panel_bg);
    DeleteObject(panel_bg);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(40, 48, 64));
    HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, grid_pen));
    for (int x = 0; x < panel_x; x += 40) {
        MoveToEx(hdc, x, 0, nullptr);
        LineTo(hdc, x, client.bottom);
    }
    for (int y = 0; y < client.bottom; y += 40) {
        MoveToEx(hdc, 0, y, nullptr);
        LineTo(hdc, panel_x, y);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(grid_pen);

    const int px = static_cast<int>(g_game.player_x);
    const int py = static_cast<int>(g_game.player_y);
    RECT player{px - 18, py - 18, px + 18, py + 18};
    HBRUSH player_brush = CreateSolidBrush(RGB(74, 158, 255));
    FillRect(hdc, &player, player_brush);
    DeleteObject(player_brush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT title_font = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT body_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT small_font = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, L"Consolas");

    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, title_font));
    draw_text(hdc, 16, 12, RGB(230, 235, 245), L"Encryptic Test Game");
    SelectObject(hdc, body_font);
    draw_text(hdc, 16, 42, RGB(160, 170, 190), L"WASD move | F1-F6 cheats | game closes on detection");

    wchar_t stats[128]{};
    swprintf_s(stats, L"Health: %d   Score: %d", g_game.health, g_game.score);
    draw_text(hdc, 16, 68, RGB(120, 220, 160), stats);

    const wchar_t* cheats[] = {
        L"F1  Memory cheat (health -> 9999)",
        L"F2  API hook (kernel32!CreateRemoteThread)",
        L"F3  Inject evil.dll",
        L"F4  Timing freeze (3s lag)",
        L"F5  Suspicious shellcode thread",
        L"F6  Synthetic input (SendInput)",
        L"F8  Restore API hook",
        L"",
        L"External tools (also detected):",
        L"  - Open x64dbg / Cheat Engine",
        L"  - Attach a debugger to this process",
        L"  - Run overlay tools not whitelisted",
    };

    int y = 110;
    for (const wchar_t* line : cheats) {
        draw_text(hdc, 16, y, RGB(130, 145, 170), line);
        y += 22;
    }

    SelectObject(hdc, title_font);
    draw_text(hdc, panel_x + 16, 12, RGB(255, 120, 90), L"DETECTION LOG");
    SelectObject(hdc, small_font);

    std::deque<ViolationEntry> copy;
    {
        std::lock_guard lock(g_violation_mutex);
        copy = g_violations;
    }

    y = 48 - g_violation_scroll * 18;
    if (copy.empty()) {
        draw_text(hdc, panel_x + 16, 52, RGB(100, 110, 130),
                  L"No violations yet. Press F1-F6 or use external tools.");
    } else {
        for (const auto& entry : copy) {
            if (y > client.bottom) {
                break;
            }
            if (y > 40) {
                std::wstring line = utf8_to_wide(entry.time + " [" + entry.severity + "] " + entry.type);
                draw_text(hdc, panel_x + 12, y, RGB(180, 190, 210), line);
                y += 16;
                if (y > 40 && y < client.bottom) {
                    std::wstring detail = utf8_to_wide(entry.message);
                    if (!entry.detail.empty()) {
                        detail += L" — " + utf8_to_wide(entry.detail);
                    }
                    draw_text(hdc, panel_x + 20, y, RGB(255, 170, 120), detail);
                }
                y += 20;
            } else {
                y += 36;
            }
        }
    }

    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    DeleteObject(body_font);
    DeleteObject(small_font);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        SetTimer(hwnd, 1, 16, nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == 1) {
            if (g_timing_cheat_pending) {
                g_timing_cheat_pending = false;
                Sleep(3000);
            }
            anticheat::Encryptic::instance().tick();
            static auto last = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            update_game(dt);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_KEYDOWN:
        g_keys[static_cast<int>(wParam) & 0xFF] = true;
        switch (wParam) {
        case VK_F1: cheat_memory_health(); break;
        case VK_F2: cheat_api_hook(); break;
        case VK_F3: cheat_load_evil_dll(); break;
        case VK_F4: cheat_timing_freeze(); break;
        case VK_F5: cheat_suspicious_thread(); break;
        case VK_F6: cheat_synthetic_input(); break;
        case VK_F8: restore_api_hook(); break;
        case VK_ESCAPE: DestroyWindow(hwnd); break;
        default: break;
        }
        return 0;

    case WM_KEYUP:
        g_keys[static_cast<int>(wParam) & 0xFF] = false;
        return 0;

    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_violation_scroll = g_violation_scroll - delta / 120;
        if (g_violation_scroll < 0) {
            g_violation_scroll = 0;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ENCRYPTIC_KICK: {
        auto* violation = reinterpret_cast<anticheat::Violation*>(lParam);
        KillTimer(hwnd, 1);
        anticheat::Encryptic::instance().stop();
        ShowWindow(hwnd, SW_HIDE);

        if (violation) {
            const anticheat::Violation copy = *violation;
            delete violation;
            show_kick_dialog(copy, g_instance);
        }

        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (anticheat::Encryptic::instance().is_running()) {
            anticheat::Encryptic::instance().stop();
        }
        g_hwnd = nullptr;
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        paint(hwnd);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, kWindowClass, L"Encryptic Test Game — press F1-F6 to trigger detections",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 680,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd) {
        return 1;
    }

    start_anticheat();
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
