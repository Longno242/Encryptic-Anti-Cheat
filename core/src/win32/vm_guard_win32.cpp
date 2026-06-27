#include <anticheat/vm_guard.hpp>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

bool registry_value_exists(HKEY root, const wchar_t* subkey, const wchar_t* value) {
    HKEY key{};
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    RegCloseKey(key);
    (void)value;
    return true;
}

} // namespace

namespace anticheat {

bool VmGuard::scan() {
    if (registry_value_exists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VMware, Inc.\\VMware Tools", nullptr)) {
        return true;
    }
    if (registry_value_exists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Oracle\\VirtualBox Guest Additions", nullptr)) {
        return true;
    }

    HKEY key{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\ACPI\\DSDT\\VBOX__", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }

    return false;
}

} // namespace anticheat

#else

namespace anticheat {

bool VmGuard::scan() {
    return false;
}

} // namespace anticheat

#endif
