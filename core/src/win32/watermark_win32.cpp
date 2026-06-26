#include <anticheat/watermark.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace Gdiplus;

ULONG_PTR g_gdiplus_token = 0;
bool g_gdiplus_ready = false;

constexpr wchar_t kWatermarkClass[] = L"EncrypticAcSplash";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 16;

struct WatermarkState {
    anticheat::WatermarkConfig cfg;
    HWND hwnd{};
    float opacity{0.f};
    float progress{0.f};
    int stage_index{0};
    DWORD start_tick{};
    DWORD fade_in_end{};
    DWORD hold_end{};
    DWORD fade_out_end{};
    bool done{false};
    float dpi_scale{1.f};
    std::vector<std::wstring> stages;
};

struct ScopedGdiplus {
    ScopedGdiplus() {
        if (!g_gdiplus_ready) {
            GdiplusStartupInput input;
            if (GdiplusStartup(&g_gdiplus_token, &input, nullptr) == Ok) {
                g_gdiplus_ready = true;
            }
        }
    }
    ~ScopedGdiplus() = default;
};

WatermarkState* state_from_hwnd(HWND hwnd) {
    return reinterpret_cast<WatermarkState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

std::wstring widen(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

float ease_out_cubic(float t) {
    const float u = 1.f - t;
    return 1.f - u * u * u;
}

int scaled(float v, float scale) {
    return static_cast<int>(std::lround(v * scale));
}

Color argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b, float opacity) {
    const auto alpha = static_cast<BYTE>(std::clamp(opacity * static_cast<float>(a), 0.f, 255.f));
    return Color(alpha, r, g, b);
}

void draw_string(Graphics& g, const std::wstring& text, Font& font, Brush& brush, REAL x, REAL y, REAL max_w) {
    if (text.empty()) {
        return;
    }
    RectF layout(x, y, max_w, 80.f);
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
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

void draw_card_shadow(Graphics& g, REAL x, REAL y, REAL w, REAL h, REAL r, float opacity) {
    for (int i = 10; i >= 1; --i) {
        const REAL spread = static_cast<REAL>(i) * 1.4f;
        const REAL lift = static_cast<REAL>(i) * 0.9f;
        const BYTE alpha = static_cast<BYTE>(std::min(28, 2 + i * 2));
        SolidBrush shadow(argb(alpha, 0, 0, 0, opacity));
        GraphicsPath shadow_path;
        build_round_rect(shadow_path, x - spread * 0.3f, y + lift, w + spread * 0.6f, h, r + 2.f);
        g.FillPath(&shadow, &shadow_path);
    }
}

void draw_lock_icon(Graphics& g, REAL x, REAL y, REAL size, Color accent, Color fg, float opacity) {
    const REAL cx = x + size * 0.5f;
    const REAL cy = y + size * 0.5f;
    const REAL r = size * 0.42f;

    Pen ring(accent, 2.f);
    ring.SetAlignment(PenAlignmentInset);
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

void paint_watermark(HWND hwnd) {
    auto* state = state_from_hwnd(hwnd);
    if (!state) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    const int w = client.right;
    const int h = client.bottom;
    if (w <= 0 || h <= 0) {
        return;
    }

    const auto& cfg = state->cfg;
    const float opacity = std::clamp(state->opacity, 0.f, 1.f);
    const float s = state->dpi_scale;

    const int card_w = scaled(static_cast<float>(cfg.card_width), s);
    const int card_h = scaled(static_cast<float>(cfg.card_height), s);
    const int card_x = (w - card_w) / 2;
    const int card_y = (h - card_h) / 2;

    Bitmap canvas(w, h, PixelFormat32bppARGB);
    Graphics g(&canvas);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetCompositingMode(CompositingModeSourceOver);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.Clear(Color(0, 0, 0, 0));

    if (cfg.fullscreen_dim) {
        SolidBrush dim(argb(90, 0, 0, 0, opacity));
        g.FillRectangle(&dim, 0, 0, w, h);
    }

    const REAL cx = static_cast<REAL>(card_x);
    const REAL cy = static_cast<REAL>(card_y);
    const REAL cw = static_cast<REAL>(card_w);
    const REAL ch = static_cast<REAL>(card_h);
    const REAL radius = 12.f * s;

    draw_card_shadow(g, cx, cy, cw, ch, radius, opacity);

    GraphicsPath card_path;
    build_round_rect(card_path, cx, cy, cw, ch, radius);
    Color top_color = argb(252, static_cast<BYTE>(cfg.card_r + 6),
                           static_cast<BYTE>(cfg.card_g + 6), static_cast<BYTE>(cfg.card_b + 6), opacity);
    Color bot_color = argb(252, cfg.card_r, cfg.card_g, cfg.card_b, opacity);
    LinearGradientBrush fill(PointF(cx, cy), PointF(cx, cy + ch), top_color, bot_color);
    g.FillPath(&fill, &card_path);

    Pen border(argb(70, 72, 78, 96, opacity), 1.f);
    border.SetAlignment(PenAlignmentInset);
    g.DrawPath(&border, &card_path);

    const REAL pad = 24.f * s;
    const REAL header_h = 64.f * s;
    const REAL logo_size = 48.f * s;
    const REAL logo_x = cx + pad;
    const REAL logo_y = cy + 20.f * s;
    const REAL text_x = cfg.show_logo_mark ? logo_x + logo_size + 16.f * s : cx + pad;
    const REAL text_w = cx + cw - pad - text_x;

    const Color accent = argb(255, cfg.accent_r, cfg.accent_g, cfg.accent_b, opacity);
    const Color text_hi = argb(255, 245, 246, 248, opacity);
    const Color text_mid = argb(255, 158, 163, 175, opacity);
    const Color text_lo = argb(255, 108, 113, 126, opacity);
    const Color text_foot = argb(255, 78, 82, 94, opacity);

    FontFamily ff(L"Segoe UI");
    Font brand(&ff, 22.f * s, FontStyleBold, UnitPixel);
    Font sub(&ff, 12.f * s, FontStyleRegular, UnitPixel);
    Font game(&ff, 14.f * s, FontStyleBold, UnitPixel);
    Font status_font(&ff, 12.f * s, FontStyleRegular, UnitPixel);
    Font foot_font(&ff, 10.f * s, FontStyleRegular, UnitPixel);

    if (cfg.show_logo_mark) {
        draw_lock_icon(g, logo_x, logo_y, logo_size, accent, text_hi, opacity);
    }

    SolidBrush hi_br(text_hi);
    SolidBrush accent_br(accent);
    draw_string(g, widen(cfg.brand_title), brand, hi_br, text_x, logo_y + 2.f * s, text_w);
    draw_string(g, widen(cfg.brand_subtitle), sub, accent_br, text_x, logo_y + 28.f * s, text_w);

    const REAL sep_y = cy + 20.f * s + header_h;
    Pen sep_pen(argb(255, 52, 55, 64, opacity), 1.f);
    g.DrawLine(&sep_pen, cx + pad, sep_y, cx + cw - pad, sep_y);

    SolidBrush mid_br(text_mid);
    SolidBrush lo_br(text_lo);
  const REAL body_y = sep_y + 14.f * s;
    draw_string(g, widen(cfg.game_title), game, mid_br, cx + pad, body_y, cw - pad * 2.f);

    const std::wstring status_text =
        state->stages.empty() ? widen(cfg.status_text) : state->stages[state->stage_index];
    draw_string(g, status_text, status_font, lo_br, cx + pad, body_y + 24.f * s, cw - pad * 2.f);

    if (cfg.show_progress_bar) {
        const REAL bar_h = 3.f * s;
        const REAL bar_y = cy + ch - pad - 28.f * s;
        const REAL bar_x = cx + pad;
        const REAL bar_w = cw - pad * 2.f;

        SolidBrush track(argb(255, 38, 41, 50, opacity));
        g.FillRectangle(&track, bar_x, bar_y, bar_w, bar_h);

        const REAL fill_w = std::max(0.f, bar_w * state->progress);
        if (fill_w > 0.5f) {
            LinearGradientBrush prog(
                PointF(bar_x, bar_y),
                PointF(bar_x + fill_w, bar_y),
                argb(255, cfg.accent_r, cfg.accent_g, cfg.accent_b, opacity),
                argb(255, static_cast<BYTE>(std::min(255, cfg.accent_r + 30)),
                     static_cast<BYTE>(std::min(255, cfg.accent_g + 20)),
                     static_cast<BYTE>(std::min(255, cfg.accent_b + 10)), opacity));
            g.FillRectangle(&prog, bar_x, bar_y, fill_w, bar_h);
        }
    }

    std::wstring foot_text = widen(cfg.footer_text);
    if (!cfg.version_text.empty()) {
        foot_text += L"   \u00B7   " + widen(cfg.version_text);
    }
    SolidBrush foot_br(text_foot);
    draw_string(g, foot_text, foot_font, foot_br, cx + pad, cy + ch - pad - 10.f * s, cw - pad * 2.f);

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP hbmp = nullptr;
    canvas.GetHBITMAP(Color(0, 0, 0, 0), &hbmp);
    HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, hbmp));

    POINT pt_dst{0, 0};
    SIZE size{w, h};
    POINT pt_src{0, 0};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, screen, &pt_dst, &size, mem, &pt_src, 0, &blend, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(hbmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

void update_animation(WatermarkState& state) {
    const DWORD now = GetTickCount();
    const auto& cfg = state.cfg;

    if (now < state.fade_in_end) {
        const float t = static_cast<float>(now - state.start_tick) /
                        static_cast<float>(std::max(1u, cfg.fade_in_ms));
        state.opacity = ease_out_cubic(std::min(1.f, t));
    } else if (now < state.hold_end) {
        state.opacity = 1.f;
        const float hold_t = static_cast<float>(now - state.fade_in_end) /
                             static_cast<float>(std::max(1u, cfg.display_duration_ms));
        state.progress = ease_out_cubic(std::min(1.f, hold_t));
        if (!state.stages.empty()) {
            state.stage_index = std::min(
                static_cast<int>(state.stages.size() - 1),
                static_cast<int>(hold_t * static_cast<float>(state.stages.size())));
        }
    } else if (now < state.fade_out_end) {
        state.progress = 1.f;
        const float t = static_cast<float>(now - state.hold_end) /
                        static_cast<float>(std::max(1u, cfg.fade_out_ms));
        state.opacity = 1.f - ease_out_cubic(std::min(1.f, t));
        if (t >= 1.f) {
            state.done = true;
        }
    } else {
        state.done = true;
    }
}

LRESULT CALLBACK watermark_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<LPCREATESTRUCT>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        SetTimer(hwnd, kTimerId, kTimerMs, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (auto* state = state_from_hwnd(hwnd)) {
            update_animation(*state);
            paint_watermark(hwnd);
            if (state->done) {
                DestroyWindow(hwnd);
            }
        }
        return 0;
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

void register_watermark_class() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = watermark_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWatermarkClass;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    RegisterClassExW(&wc);
    registered = true;
}

void build_default_stages(WatermarkState& state) {
    if (!state.cfg.loading_stages.empty()) {
        for (const auto& line : state.cfg.loading_stages) {
            state.stages.push_back(widen(line));
        }
        return;
    }
    state.stages = {
        L"Starting protection service",
        L"Scanning runtime environment",
        L"Validating module integrity",
        L"Establishing secure session",
    };
}

void run_watermark(const anticheat::WatermarkConfig& config) {
    ScopedGdiplus gdi;
    if (!g_gdiplus_ready) {
        return;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    register_watermark_class();

    WatermarkState state;
    state.cfg = config;
    state.opacity = 0.f;
    state.start_tick = GetTickCount();
    state.fade_in_end = state.start_tick + config.fade_in_ms;
    state.hold_end = state.fade_in_end + config.display_duration_ms;
    state.fade_out_end = state.hold_end + config.fade_out_ms;
    build_default_stages(state);

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);

    DWORD ex_style = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    if (config.topmost) {
        ex_style |= WS_EX_TOPMOST;
    }

    HWND hwnd = CreateWindowExW(
        ex_style,
        kWatermarkClass,
        L"Encryptic",
        WS_POPUP,
        0,
        0,
        screen_w,
        screen_h,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        &state);

    if (!hwnd) {
        return;
    }

    state.hwnd = hwnd;
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        if (auto* get_dpi = reinterpret_cast<UINT(WINAPI*)(HWND)>(GetProcAddress(user32, "GetDpiForWindow"))) {
            state.dpi_scale = static_cast<float>(get_dpi(hwnd)) / 96.f;
        }
    }

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

} // namespace

namespace anticheat {

void Watermark::show_blocking(const WatermarkConfig& config) {
    if (!config.enabled) {
        return;
    }
    run_watermark(config);
}

void Watermark::show_preview(const WatermarkConfig& config) {
    run_watermark(config);
}

} // namespace anticheat

#else

namespace anticheat {

void Watermark::show_blocking(const WatermarkConfig&) {}
void Watermark::show_preview(const WatermarkConfig&) {}

} // namespace anticheat

#endif
