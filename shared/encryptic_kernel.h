#pragma once

/*
 * Shared definitions between EncrypticGuard.sys (kernel) and encryptic_core (user-mode).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENCRYPTIC_KERNEL_DEVICE_NAME L"\\\\.\\EncrypticGuard"
#define ENCRYPTIC_KERNEL_DEVICE_PATH L"\\Device\\EncrypticGuard"
#define ENCRYPTIC_KERNEL_DOS_DEVICE  L"\\DosDevices\\EncrypticGuard"

#define ENCRYPTIC_KERNEL_IOCTL_BASE 0x800

#define CTL_CODE_ENCRYPTIC(id) \
    (0x22u << 16) | (0x0u << 14) | (0x0u << 12) | ((ENCRYPTIC_KERNEL_IOCTL_BASE + (id)) << 2)

#define IOCTL_ENCRYPTIC_GET_VERSION     CTL_CODE_ENCRYPTIC(0x00)
#define IOCTL_ENCRYPTIC_SET_TARGET      CTL_CODE_ENCRYPTIC(0x01)
#define IOCTL_ENCRYPTIC_UPDATE_CONFIG   CTL_CODE_ENCRYPTIC(0x02)
#define IOCTL_ENCRYPTIC_POLL_EVENTS     CTL_CODE_ENCRYPTIC(0x03)
#define IOCTL_ENCRYPTIC_GET_STATUS      CTL_CODE_ENCRYPTIC(0x04)
#define IOCTL_ENCRYPTIC_CLEAR_TARGET    CTL_CODE_ENCRYPTIC(0x05)

#define ENCRYPTIC_KERNEL_VERSION 0x00010100u

#define ENCRYPTIC_MAX_BLOCKED_NAMES   48
#define ENCRYPTIC_BLOCKED_NAME_CHARS  64
#define ENCRYPTIC_EVENT_DETAIL_CHARS  128
#define ENCRYPTIC_MAX_EVENTS_PER_POLL 32

/* Feature flags returned by GET_VERSION */
#define ENCRYPTIC_FEATURE_PROCESS_NOTIFY  0x01u
#define ENCRYPTIC_FEATURE_IMAGE_NOTIFY    0x02u
#define ENCRYPTIC_FEATURE_OB_CALLBACKS    0x04u
#define ENCRYPTIC_FEATURE_SESSION_SCOPED  0x08u
#define ENCRYPTIC_FEATURE_TRUSTED_WHITELIST 0x10u

/* Target flags — default is scoped (safe for multi-game PCs) */
#define ENCRYPTIC_TARGET_PROTECT_HANDLES   0x01u
#define ENCRYPTIC_TARGET_BLOCK_DRIVERS     0x02u
#define ENCRYPTIC_TARGET_BLOCK_MODULES     0x04u
#define ENCRYPTIC_TARGET_BLOCK_GLOBAL_CHEATS 0x08u /* OFF by default: avoids affecting other games */

typedef enum _ENCRYPTIC_KERNEL_EVENT_TYPE {
    EncrypticKernelEventNone = 0,
    EncrypticKernelBlockedProcess = 1,
    EncrypticKernelSuspiciousImage = 2,
    EncrypticKernelHandleStripped = 3,
    EncrypticKernelBlockedDriver = 4,
    EncrypticKernelProtectedProcessExit = 5,
    EncrypticKernelSessionStarted = 6,
    EncrypticKernelSessionCleared = 7,
} ENCRYPTIC_KERNEL_EVENT_TYPE;

typedef struct _ENCRYPTIC_KERNEL_VER_INFO {
    uint32_t driver_version;
    uint32_t feature_flags;
} ENCRYPTIC_KERNEL_VER_INFO;

typedef struct _ENCRYPTIC_KERNEL_TARGET {
    uint32_t process_id;
    uint32_t client_pid;
    uint32_t flags;
    uint64_t session_token;
} ENCRYPTIC_KERNEL_TARGET;

typedef struct _ENCRYPTIC_KERNEL_SESSION {
    uint64_t session_token;
} ENCRYPTIC_KERNEL_SESSION;

typedef struct _ENCRYPTIC_KERNEL_CONFIG {
    uint32_t blocked_process_count;
    uint32_t blocked_driver_count;
    uint32_t blocked_module_count;
    wchar_t blocked_processes[ENCRYPTIC_MAX_BLOCKED_NAMES][ENCRYPTIC_BLOCKED_NAME_CHARS];
    wchar_t blocked_drivers[ENCRYPTIC_MAX_BLOCKED_NAMES][ENCRYPTIC_BLOCKED_NAME_CHARS];
    wchar_t blocked_modules[ENCRYPTIC_MAX_BLOCKED_NAMES][ENCRYPTIC_BLOCKED_NAME_CHARS];
} ENCRYPTIC_KERNEL_CONFIG;

typedef struct _ENCRYPTIC_KERNEL_EVENT {
    uint32_t type;
    uint32_t target_pid;
    uint32_t source_pid;
    uint64_t timestamp_100ns;
    wchar_t detail[ENCRYPTIC_EVENT_DETAIL_CHARS];
} ENCRYPTIC_KERNEL_EVENT;

typedef struct _ENCRYPTIC_KERNEL_EVENT_BATCH {
    uint32_t count;
    ENCRYPTIC_KERNEL_EVENT events[ENCRYPTIC_MAX_EVENTS_PER_POLL];
} ENCRYPTIC_KERNEL_EVENT_BATCH;

typedef struct _ENCRYPTIC_KERNEL_STATUS {
    uint32_t driver_active;
    uint32_t session_active;
    uint32_t target_pid;
    uint32_t client_pid;
    uint32_t callbacks_registered;
    uint32_t events_pending;
    uint32_t handles_stripped;
    uint32_t processes_blocked;
    uint32_t images_blocked;
    uint32_t target_flags;
} ENCRYPTIC_KERNEL_STATUS;

#ifdef __cplusplus
}
#endif
