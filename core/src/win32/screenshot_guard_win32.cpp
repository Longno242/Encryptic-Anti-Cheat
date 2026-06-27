#include <anticheat/screenshot_guard.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <sstream>
#include <vector>

namespace anticheat {

void ScreenshotGuard::configure(const ScreenshotConfig& config) {
    cfg_ = config;
}

void ScreenshotGuard::capture_on_violation(const Violation& violation) {
    if (!cfg_.on_critical || violation.severity != ViolationSeverity::Critical) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(cfg_.output_dir, ec);

    const int w = GetSystemMetrics(SM_CXSCREEN);
    const int h = GetSystemMetrics(SM_CYSCREEN);
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, bmp));
    BitBlt(mem, 0, 0, w, h, screen, 0, 0, SRCCOPY);

    const auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    std::ostringstream path;
    path << cfg_.output_dir << "\\capture_" << ts << ".bmp";

    BITMAPFILEHEADER bfh{};
    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(bih);
    bih.biWidth = w;
    bih.biHeight = -h;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    BITMAP bmp_info{};
    GetObject(bmp, sizeof(bmp), &bmp_info);
    const DWORD image_size = static_cast<DWORD>(bmp_info.bmWidthBytes * bmp_info.bmHeight);
    std::vector<BYTE> pixels(image_size);
    GetDIBits(mem, bmp, 0, h, pixels.data(), reinterpret_cast<BITMAPINFO*>(&bih), DIB_RGB_COLORS);

    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + image_size;

    FILE* file = nullptr;
    if (fopen_s(&file, path.str().c_str(), "wb") == 0 && file) {
        fwrite(&bfh, sizeof(bfh), 1, file);
        fwrite(&bih, sizeof(bih), 1, file);
        fwrite(pixels.data(), 1, image_size, file);
        fclose(file);
    }

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

} // namespace anticheat

#else

namespace anticheat {

void ScreenshotGuard::configure(const ScreenshotConfig&) {}
void ScreenshotGuard::capture_on_violation(const Violation&) {}

} // namespace anticheat

#endif
