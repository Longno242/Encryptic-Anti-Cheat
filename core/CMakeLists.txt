cmake_minimum_required(VERSION 3.16)

set(ENCRYPTIC_CORE_SOURCES
    src/encryptic.cpp
    src/config.cpp
    src/config_loader.cpp
    src/platform.cpp
    src/win32/platform_win32.cpp
    src/win32/watermark_win32.cpp
    src/win32/telemetry_win32.cpp
    src/win32/heartbeat_guard_win32.cpp
    src/win32/integrity_guard_win32.cpp
    src/win32/screenshot_guard_win32.cpp
    src/win32/watchdog_guard_win32.cpp
    src/win32/dll_guard_win32.cpp
    src/win32/debugger_guard_win32.cpp
    src/win32/hook_guard_win32.cpp
    src/win32/memory_guard_win32.cpp
    src/win32/module_whitelist_win32.cpp
    src/win32/thread_guard_win32.cpp
    src/win32/timing_guard_win32.cpp
    src/win32/handle_guard_win32.cpp
    src/win32/process_guard_win32.cpp
    src/win32/threat_intel_guard_win32.cpp
    src/win32/injection_guard_win32.cpp
    src/win32/anti_bypass_guard_win32.cpp
    src/win32/hash_guard_win32.cpp
    src/win32/pipe_guard_win32.cpp
    src/win32/kernel_guard_win32.cpp
    src/win32/vm_guard_win32.cpp
    src/win32/overlay_guard_win32.cpp
    src/win32/input_guard_win32.cpp
)

add_library(encryptic_core_static STATIC ${ENCRYPTIC_CORE_SOURCES})
target_include_directories(encryptic_core_static PUBLIC include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
target_compile_definitions(encryptic_core_static PRIVATE _CRT_SECURE_NO_WARNINGS)

if(WIN32)
    target_link_libraries(encryptic_core_static PRIVATE psapi wintrust gdi32 user32 gdiplus winhttp crypt32 advapi32 version)
endif()

if(ENCRYPTIC_BUILD_SHARED)
    add_library(encryptic_core SHARED ${ENCRYPTIC_CORE_SOURCES})
    target_include_directories(encryptic_core PUBLIC include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
    target_compile_definitions(encryptic_core PRIVATE ENCRYPTIC_BUILD_DLL _CRT_SECURE_NO_WARNINGS)
    if(WIN32)
        target_link_libraries(encryptic_core PRIVATE psapi wintrust gdi32 user32 gdiplus winhttp crypt32 advapi32 version)
    endif()
    set_target_properties(encryptic_core PROPERTIES OUTPUT_NAME encryptic_core)
endif()
