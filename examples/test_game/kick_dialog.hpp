#pragma once

#include <anticheat/types.hpp>

#include <windows.h>

void show_kick_dialog(const anticheat::Violation& violation, HINSTANCE instance);
