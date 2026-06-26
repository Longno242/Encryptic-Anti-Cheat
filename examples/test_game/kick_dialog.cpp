#include "kick_dialog.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace {

using namespace Gdiplus;

ULONG_PTR g_gdiplus_token = 0;
bool g_gdiplus_ready = false;

constexpr wchar_t kKickClass[] = L"EncrypticKickDialogModern";
constexpr int kDlgW = 560;
constexpr int kDlgH = 430;

struct KickUiState {
    anticheat::Violation violation;
    RECT close_btn{};
    bool close_hover = false;
};

KickUiState* state_from(HWND hwnd) {
    return reinterpret_cast<KickUiState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

std::wstring widen(const std::string& text) {
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

void draw_string(Graphics& g, const std::wstring& text, Font& font, Brush& brush, REAL x, REAL y, REAL max_w) {
    if (text.empty()) {
        return;
    }
    RectF layout(x, y, max_w, 120.f);
    StringFormat fmt;
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, layout, &fmt, &brush);
}

void build_round_rect(GraphicsPath& path, REAL x, REAL y, REAL w, REAL h, REAL r) {
    const REAL d = r * 2.f;
    path.Reset();
    path.AddArc(x, y, d, d, 180.f, 90.f);
    path.AddArc(x + w - d, y, d, d, 270.f, 90.f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.f, 90.f);
    path.AddArc(x, y + h - d, d, d, 90.f, 90.f);
    path.CloseFigure();
}

void draw_lock_icon(Graphics& g, REAL x, REAL y, REAL size, Color accent, Color fg) {
    const REAL cx = x + size * 0.5f;
    const REAL cy = y + size * 0.5f;
    const REAL r = size * 0.42f;

    Pen ring(accent, 2.f);
    g.DrawEllipse(&ring, x + 2.f, y + 2.f, size - 4.f, size - 4.f);

    GraphicsPath shackle;
    const REAL sh_w = r * 0.72f;
    const REAL sh_h = r * 0.55f;
    shackle.AddArc(cx - sh_w, cy - r * 0.15f, sh_w * 2.f, sh_h * 2.f, 180.f, 180.f);
    Pen sh_pen(fg, 2.6f);
    sh_pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    g.DrawPath(&sh_pen, &shackle);

    SolidBrush body(fg);
    GraphicsPath shield;
    const REAL bw = r * 0.55f;
    const REAL bh = r * 0.62f;
    shield.AddLine(cx, cy + bh * 0.35f, cx - bw, cy - bh * 0.1f);
    shield.AddLine(cx - bw, cy - bh * 0.1f, cx, cy - bh * 0.55f);
    shield.AddLine(cx, cy - bh * 0.55f, cx + bw, cy - bh * 0.1f);
    shield.AddLine(cx + bw, cy - bh * 0.1f, cx, cy + bh * 0.35f);
    shield.CloseFigure();
    g.FillPath(&body, &shield);
}

std::wstring friendly_headline(const anticheat::Violation& v) {
    switch (v.type) {
    case anticheat::ViolationType::MemoryTampered:
        return L"Memory tampering detected";
    case anticheat::ViolationType::ApiHooked:
        return L"Unauthorized API hook detected";
    case anticheat::ViolationType::UnknownDll:
    case anticheat::ViolationType::ModuleNotWhitelisted:
        return L"Unauthorized module detected";
    case anticheat::ViolationType::DebuggerDetected:
        return L"Debugger detected";
    case anticheat::ViolationType::CheatProcessDetected:
        return L"Cheat software detected";
    case anticheat::ViolationType::TimingAnomaly:
        return L"Timing manipulation detected";
    case anticheat::ViolationType::SuspiciousThread:
        return L"Suspicious thread detected";
    case anticheat::ViolationType::SyntheticInputDetected:
        return L"Synthetic input detected";
    case anticheat::ViolationType::ExternalHandle:
        return L"External process interference";
    case anticheat::ViolationType::VirtualMachineDetected:
        return L"Virtual machine environment detected";
    case anticheat::ViolationType::SuspiciousOverlay:
        return L"Suspicious overlay detected";
    case anticheat::ViolationType::SelfIntegrityFailed:
    case anticheat::ViolationType::TamperDetected:
        return L"Integrity violation detected";
    case anticheat::ViolationType::WatchdogFailure:
        return L"Protection watchdog failure";
    case anticheat::ViolationType::HeartbeatMissed:
        return L"Server heartbeat lost";
    default:
        return widen(v.message.empty() ? "Security violation detected" : v.message);
    }
}

std::wstring friendly_body(const anticheat::Violation& v) {
    if (!v.detail.empty()) {
        return widen(v.detail);
    }
    return widen(v.message);
}

std::wstring detection_badge(const anticheat::Violation& v) {
    switch (v.type) {
    case anticheat::ViolationType::MemoryTampered: return L"MEMORY GUARD";
    case anticheat::ViolationType::ApiHooked: return L"HOOK GUARD";
    case anticheat::ViolationType::UnknownDll: return L"DLL GUARD";
    case anticheat::ViolationType::ModuleNotWhitelisted: return L"MODULE WHITELIST";
    case anticheat::ViolationType::DebuggerDetected: return L"DEBUGGER GUARD";
    case anticheat::ViolationType::CheatProcessDetected: return L"PROCESS GUARD";
    case anticheat::ViolationType::TimingAnomaly: return L"TIMING GUARD";
    case anticheat::ViolationType::SuspiciousThread: return L"THREAD GUARD";
    case anticheat::ViolationType::SyntheticInputDetected: return L"INPUT GUARD";
    case anticheat::ViolationType::ExternalHandle: return L"HANDLE GUARD";
    case anticheat::ViolationType::VirtualMachineDetected: return L"VM GUARD";
    case anticheat::ViolationType::SuspiciousOverlay: return L"OVERLAY GUARD";
    case anticheat::ViolationType::SelfIntegrityFailed: return L"INTEGRITY GUARD";
    case anticheat::ViolationType::WatchdogFailure: return L"WATCHDOG";
    case anticheat::ViolationType::HeartbeatMissed: return L"HEARTBEAT";
    default: return L"ENCRYPTIC";
    }
}

void paint_kick(HWND hwnd) {
    auto* state = state_from(hwnd);
    if (!state) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    const int w = client.right;
    const int h = client.bottom;

    Bitmap canvas(w, h, PixelFormat32bppARGB);
    Graphics g(&canvas);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.Clear(Color(255, 10, 12, 18));

    const REAL pad = 28.f;
    const REAL card_x = pad;
    const REAL card_y = pad;
    const REAL card_w = static_cast<REAL>(w) - pad * 2.f;
    const REAL card_h = static_cast<REAL>(h) - pad * 2.f;
    const REAL radius = 14.f;

    GraphicsPath card_path;
    build_round_rect(card_path, card_x, card_y, card_w, card_h, radius);
    LinearGradientBrush card_fill(
        PointF(card_x, card_y), PointF(card_x, card_y + card_h),
        Color(255, 30, 33, 42), Color(255, 20, 22, 30));
    g.FillPath(&card_fill, &card_path);

    Pen border(Color(90, 74, 158, 255), 1.2f);
    g.DrawPath(&border, &card_path);

    const Color accent(255, 74, 158, 255);
    const Color text_hi(255, 245, 247, 250);
    const Color text_mid(255, 160, 168, 182);
    const Color warn(255, 255, 120, 96);
    const Color detail_col(255, 255, 196, 140);

    FontFamily ff(L"Segoe UI");
    Font brand(&ff, 26.f, FontStyleBold, UnitPixel);
    Font sub(&ff, 13.f, FontStyleRegular, UnitPixel);
    Font headline(&ff, 20.f, FontStyleBold, UnitPixel);
    Font body(&ff, 14.f, FontStyleRegular, UnitPixel);
    Font badge_font(&ff, 11.f, FontStyleBold, UnitPixel);
    Font btn_font(&ff, 14.f, FontStyleBold, UnitPixel);
    Font reason_title(&ff, 15.f, FontStyleBold, UnitPixel);

    draw_lock_icon(g, card_x + 24.f, card_y + 22.f, 52.f, accent, text_hi);

    SolidBrush hi_br(text_hi);
    SolidBrush accent_br(accent);
    draw_string(g, L"Encryptic", brand, accent_br, card_x + 88.f, card_y + 24.f, card_w - 120.f);
    draw_string(g, L"Anti-Cheat", sub, hi_br, card_x + 88.f, card_y + 56.f, card_w - 120.f);

    SolidBrush warn_br(warn);
    draw_string(g, L"Session closed", headline, warn_br, card_x + 24.f, card_y + 96.f, card_w - 48.f);

    SolidBrush mid_br(text_mid);
    draw_string(g, L"Your game was closed to protect this session.", body, mid_br, card_x + 24.f, card_y + 128.f,
                card_w - 48.f);

    const std::wstring headline_text = friendly_headline(state->violation);
    const std::wstring body_text = friendly_body(state->violation);
    const std::wstring badge = detection_badge(state->violation);

    REAL box_x = card_x + 24.f;
    REAL box_y = card_y + 168.f;
    REAL box_w = card_w - 48.f;
    REAL box_h = 150.f;

    GraphicsPath box_path;
    build_round_rect(box_path, box_x, box_y, box_w, box_h, 10.f);
    SolidBrush box_fill(Color(255, 16, 18, 26));
    g.FillPath(&box_fill, &box_path);
    Pen box_border(Color(70, 255, 140, 90), 1.f);
    g.DrawPath(&box_border, &box_path);

    SolidBrush detail_br(detail_col);
    draw_string(g, headline_text, reason_title, detail_br, box_x + 16.f, box_y + 14.f, box_w - 32.f);
    draw_string(g, body_text, body, mid_br, box_x + 16.f, box_y + 42.f, box_w - 32.f);

    GraphicsPath badge_path;
    const REAL badge_w = 140.f;
    const REAL badge_h = 24.f;
    const REAL badge_x = box_x + 16.f;
    const REAL badge_y = box_y + box_h - badge_h - 14.f;
    build_round_rect(badge_path, badge_x, badge_y, badge_w, badge_h, 6.f);
    SolidBrush badge_fill(Color(255, 40, 70, 120));
    g.FillPath(&badge_fill, &badge_path);
    SolidBrush badge_br(text_hi);
    StringFormat center;
    center.SetAlignment(StringAlignmentCenter);
    center.SetLineAlignment(StringAlignmentCenter);
    RectF badge_rect(badge_x, badge_y, badge_w, badge_h);
    g.DrawString(badge.c_str(), static_cast<INT>(badge.size()), &badge_font, badge_rect, &center, &badge_br);

    const REAL btn_w = 132.f;
    const REAL btn_h = 40.f;
    const REAL btn_x = card_x + card_w - btn_w - 24.f;
    const REAL btn_y = card_y + card_h - btn_h - 22.f;

    state->close_btn = {
        static_cast<LONG>(btn_x), static_cast<LONG>(btn_y),
        static_cast<LONG>(btn_x + btn_w), static_cast<LONG>(btn_y + btn_h)};

    GraphicsPath btn_path;
    build_round_rect(btn_path, btn_x, btn_y, btn_w, btn_h, 8.f);
    const Color btn_top = state->close_hover ? Color(255, 110, 180, 255) : Color(255, 74, 158, 255);
    const Color btn_bot = state->close_hover ? Color(255, 58, 130, 230) : Color(255, 48, 118, 220);
    LinearGradientBrush btn_fill(PointF(btn_x, btn_y), PointF(btn_x, btn_y + btn_h), btn_top, btn_bot);
    g.FillPath(&btn_fill, &btn_path);
    SolidBrush btn_text(text_hi);
    RectF btn_rect(btn_x, btn_y, btn_w, btn_h);
    g.DrawString(L"Close", 5, &btn_font, btn_rect, &center, &btn_text);

    HDC screen = GetDC(hwnd);
    Graphics screen_g(screen);
    screen_g.DrawImage(&canvas, 0, 0);
    ReleaseDC(hwnd, screen);
}

LRESULT CALLBACK KickWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        paint_kick(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        auto* state = state_from(hwnd);
        if (!state) {
            return 0;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const bool hover = x >= state->close_btn.left && x <= state->close_btn.right &&
                           y >= state->close_btn.top && y <= state->close_btn.bottom;
        if (hover != state->close_hover) {
            state->close_hover = hover;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE: {
        auto* state = state_from(hwnd);
        if (state && state->close_hover) {
            state->close_hover = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        auto* state = state_from(hwnd);
        if (!state) {
            return 0;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (x >= state->close_btn.left && x <= state->close_btn.right &&
            y >= state->close_btn.top && y <= state->close_btn.bottom) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY: {
        auto* state = state_from(hwnd);
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

void show_kick_dialog(const anticheat::Violation& violation, HINSTANCE instance) {
    if (!g_gdiplus_ready) {
        GdiplusStartupInput input;
        if (GdiplusStartup(&g_gdiplus_token, &input, nullptr) == Ok) {
            g_gdiplus_ready = true;
        }
    }

    auto* state = new KickUiState{};
    state->violation = violation;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = KickWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kKickClass;
    RegisterClassExW(&wc);

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST, kKickClass, L"Encryptic Anti-Cheat",
        WS_POPUP, (screen_w - kDlgW) / 2, (screen_h - kDlgH) / 2, kDlgW, kDlgH,
        nullptr, nullptr, instance, state);

    if (!hwnd) {
        delete state;
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg{};
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
