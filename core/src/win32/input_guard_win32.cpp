#include <anticheat/input_guard.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace anticheat {

void InputGuard::prime() {
    consecutive_hits_ = 0;
}

bool InputGuard::scan() {
    // Do not sample WASD — GetKeyState lags behind GetAsyncKeyState during normal gameplay
    // and causes false positives. SendInput cheats typically target other keys / mouse.
    const int vk_samples[] = {
        VK_SHIFT, VK_CONTROL, VK_MENU,
        VK_LBUTTON, VK_RBUTTON, VK_MBUTTON,
        'J', 'K', 'F', 'E', 'Q', 'R',
    };

    int synthetic_presses = 0;

    for (int vk : vk_samples) {
        const bool async_down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool key_down = (GetKeyState(vk) & 0x8000) != 0;

        // SendInput sets async state without updating the thread key queue.
        if (async_down && !key_down) {
            ++synthetic_presses;
        }
    }

    if (synthetic_presses >= 2) {
        ++consecutive_hits_;
    } else {
        consecutive_hits_ = 0;
    }

    return consecutive_hits_ >= 3;
}

} // namespace anticheat

#else

namespace anticheat {

void InputGuard::prime() {}
bool InputGuard::scan() { return false; }

} // namespace anticheat

#endif
