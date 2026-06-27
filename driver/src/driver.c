/*
 * EncrypticGuard.sys — kernel-mode anti-cheat companion driver.
 *
 * Features:
 *   - Process creation notify (blocked cheat/debugger processes)
 *   - Image load notify (blocked drivers/modules into protected process)
 *   - ObRegisterCallbacks (strip VM read/write on protected game process)
 *   - Event ring buffer polled by user-mode encryptic_core
 *
 * Requires test signing or EV code signing. See docs/kernel-driver.md
 */

#include <ntddk.h>
#include <ntstrsafe.h>

#include "../../shared/encryptic_kernel.h"

#define ENCRYPTIC_POOL_TAG 'cgnE'
#define ENCRYPTIC_EVENT_RING_SIZE 128

typedef struct _ENCRYPTIC_DEVICE_EXTENSION {
    PDEVICE_OBJECT device_object;
    UNICODE_STRING device_name;
    UNICODE_STRING dos_device_name;

    volatile HANDLE target_pid;
    volatile HANDLE client_pid;
    volatile ULONG target_flags;
    volatile ULONGLONG session_token;
    volatile LONG session_active;

    ENCRYPTIC_KERNEL_CONFIG config;
    KSPIN_LOCK config_lock;

    ENCRYPTIC_KERNEL_EVENT event_ring[ENCRYPTIC_EVENT_RING_SIZE];
    volatile LONG event_head;
    volatile LONG event_tail;
    KSPIN_LOCK event_lock;

    volatile LONG handles_stripped;
    volatile LONG processes_blocked;
    volatile LONG images_blocked;

    PVOID process_notify_handle;
    PVOID image_notify_handle;
    PVOID ob_callback_handle;
} ENCRYPTIC_DEVICE_EXTENSION, *PENCRYPTIC_DEVICE_EXTENSION;

static PENCRYPTIC_DEVICE_EXTENSION g_ext = NULL;

static VOID EncrypticPushEvent(_In_ ENCRYPTIC_KERNEL_EVENT_TYPE type,
                               _In_ HANDLE target_pid,
                               _In_ HANDLE source_pid,
                               _In_opt_ PCWSTR detail)
{
    if (!g_ext) {
        return;
    }

    KIRQL old_irql;
    KeAcquireSpinLock(&g_ext->event_lock, &old_irql);

    const LONG head = g_ext->event_head;
    const LONG next = (head + 1) % ENCRYPTIC_EVENT_RING_SIZE;

    if (next == g_ext->event_tail) {
        KeReleaseSpinLock(&g_ext->event_lock, old_irql);
        return;
    }

    ENCRYPTIC_KERNEL_EVENT* ev = &g_ext->event_ring[head];
    RtlZeroMemory(ev, sizeof(*ev));
    ev->type = (uint32_t)type;
    ev->target_pid = HandleToUlong(target_pid);
    ev->source_pid = HandleToUlong(source_pid);

    LARGE_INTEGER ts;
    KeQuerySystemTime(&ts);
    ev->timestamp_100ns = (uint64_t)ts.QuadPart;

    if (detail) {
        RtlStringCchCopyW(ev->detail, ENCRYPTIC_EVENT_DETAIL_CHARS, detail);
    }

    g_ext->event_head = next;
    KeReleaseSpinLock(&g_ext->event_lock, old_irql);
}

static BOOLEAN EncrypticSessionActive(VOID)
{
    return g_ext && g_ext->session_active && g_ext->target_pid != NULL;
}

static BOOLEAN EncrypticIsTrustedProcess(_In_ HANDLE ProcessId)
{
    if (ProcessId == NULL || ProcessId == PsGetCurrentProcessId()) {
        return TRUE;
    }

    PEPROCESS process = NULL;
    if (!NT_SUCCESS(PsLookupProcessByProcessId(ProcessId, &process))) {
        return FALSE;
    }

    const CHAR* image = PsGetProcessImageFileName(process);
    ObDereferenceObject(process);
    if (!image) {
        return FALSE;
    }

    ANSI_STRING name;
    RtlInitAnsiString(&name, image);

    static const PCSTR trusted[] = {
        "System", "csrss.exe", "lsass.exe", "services.exe", "svchost.exe",
        "steam.exe", "steamservice.exe", "RobloxPlayerBeta.exe", "RobloxPlayer.exe",
        "EasyAntiCheat.exe", "EasyAntiCheat_EOS.exe", "BEService.exe", "BattlEye",
        "vgc.exe", "vgtray.exe", "FaceitClient.exe", "MsMpEng.exe", "SearchIndexer.exe",
        "explorer.exe", "dwm.exe", "winlogon.exe",
    };

    for (ULONG i = 0; i < ARRAYSIZE(trusted); ++i) {
        ANSI_STRING needle;
        RtlInitAnsiString(&needle, trusted[i]);
        if (RtlCompareString(&name, &needle, TRUE) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static VOID EncrypticClearSession(_In_opt_ PCWSTR reason)
{
    if (!g_ext) {
        return;
    }

    const HANDLE old_target = g_ext->target_pid;
    g_ext->target_pid = NULL;
    g_ext->client_pid = NULL;
    g_ext->session_token = 0;
    g_ext->target_flags = 0;
    g_ext->session_active = 0;

    if (old_target) {
        EncrypticPushEvent(EncrypticKernelSessionCleared, old_target, PsGetCurrentProcessId(), reason);
    }
}

static BOOLEAN EncrypticContainsInsensitive(_In_ PCUNICODE_STRING haystack, _In_ PCWSTR needle)
{
    if (!haystack || !haystack->Buffer || haystack->Length == 0 || !needle) {
        return FALSE;
    }

    UNICODE_STRING needle_us;
    RtlInitUnicodeString(&needle_us, needle);

    UNICODE_STRING haystack_lower;
    UNICODE_STRING needle_lower;

    USHORT max_chars = haystack->Length / sizeof(WCHAR) + 1;
    PWCHAR hay_buf = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, max_chars * sizeof(WCHAR), ENCRYPTIC_POOL_TAG);
    PWCHAR needle_buf = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, (needle_us.Length / sizeof(WCHAR) + 1) * sizeof(WCHAR), ENCRYPTIC_POOL_TAG);
    if (!hay_buf || !needle_buf) {
        if (hay_buf) ExFreePoolWithTag(hay_buf, ENCRYPTIC_POOL_TAG);
        if (needle_buf) ExFreePoolWithTag(needle_buf, ENCRYPTIC_POOL_TAG);
        return FALSE;
    }

    RtlZeroMemory(hay_buf, max_chars * sizeof(WCHAR));
    RtlCopyMemory(hay_buf, haystack->Buffer, haystack->Length);
    for (USHORT i = 0; i < max_chars - 1 && hay_buf[i]; ++i) {
        if (hay_buf[i] >= L'A' && hay_buf[i] <= L'Z') {
            hay_buf[i] = (WCHAR)(hay_buf[i] + (L'a' - L'A'));
        }
    }

    RtlStringCchCopyW(needle_buf, needle_us.Length / sizeof(WCHAR) + 1, needle);
    for (USHORT i = 0; needle_buf[i]; ++i) {
        if (needle_buf[i] >= L'A' && needle_buf[i] <= L'Z') {
            needle_buf[i] = (WCHAR)(needle_buf[i] + (L'a' - L'A'));
        }
    }

    haystack_lower.Buffer = hay_buf;
    haystack_lower.Length = haystack->Length;
    haystack_lower.MaximumLength = max_chars * sizeof(WCHAR);

    RtlInitUnicodeString(&needle_lower, needle_buf);

    BOOLEAN found = FALSE;
    if (haystack_lower.Length >= needle_lower.Length) {
        for (USHORT i = 0; i <= (haystack_lower.Length - needle_lower.Length) / sizeof(WCHAR); ++i) {
            UNICODE_STRING slice;
            slice.Buffer = haystack_lower.Buffer + i;
            slice.Length = needle_lower.Length;
            slice.MaximumLength = slice.Length;
            if (RtlCompareUnicodeString(&slice, &needle_lower, TRUE) == 0) {
                found = TRUE;
                break;
            }
        }
    }

    ExFreePoolWithTag(hay_buf, ENCRYPTIC_POOL_TAG);
    ExFreePoolWithTag(needle_buf, ENCRYPTIC_POOL_TAG);
    return found;
}

static BOOLEAN EncrypticMatchesBlocklist(_In_ PCUNICODE_STRING name, _In_ BOOLEAN drivers)
{
    if (!g_ext || !name) {
        return FALSE;
    }

    KIRQL old_irql;
    KeAcquireSpinLock(&g_ext->config_lock, &old_irql);

    const uint32_t count = drivers ? g_ext->config.blocked_driver_count : g_ext->config.blocked_process_count;
    BOOLEAN matched = FALSE;

    for (uint32_t i = 0; i < count && i < ENCRYPTIC_MAX_BLOCKED_NAMES; ++i) {
        PCWSTR blocked = drivers ? g_ext->config.blocked_drivers[i] : g_ext->config.blocked_processes[i];
        if (blocked[0] && EncrypticContainsInsensitive(name, blocked)) {
            matched = TRUE;
            break;
        }
    }

    KeReleaseSpinLock(&g_ext->config_lock, old_irql);
    return matched;
}

static BOOLEAN EncrypticMatchesModuleBlocklist(_In_ PCUNICODE_STRING name)
{
    if (!g_ext || !name) {
        return FALSE;
    }

    KIRQL old_irql;
    KeAcquireSpinLock(&g_ext->config_lock, &old_irql);

    BOOLEAN matched = FALSE;
    for (uint32_t i = 0; i < g_ext->config.blocked_module_count && i < ENCRYPTIC_MAX_BLOCKED_NAMES; ++i) {
        if (g_ext->config.blocked_modules[i][0] &&
            EncrypticContainsInsensitive(name, g_ext->config.blocked_modules[i])) {
            matched = TRUE;
            break;
        }
    }

    KeReleaseSpinLock(&g_ext->config_lock, old_irql);
    return matched;
}

static VOID EncrypticLoadDefaultConfig(_Inout_ PENCRYPTIC_DEVICE_EXTENSION ext)
{
    static const WCHAR* default_processes[] = {
        L"cheatengine", L"x64dbg", L"x32dbg", L"processhacker", L"ida64", L"ida",
        L"reclass", L"extremeinjector", L"xenos", L"dbk64", L"dbk32",
    };
    static const WCHAR* default_drivers[] = {
        L"dbk64", L"dbk32", L"cedriver", L"cedriver73", L"kprocesshacker", L"dbvm",
    };
    static const WCHAR* default_modules[] = {
        L"dbk64.dll", L"dbk32.dll", L"cedebug.dll", L"speedhack.dll", L"blackbone.dll",
    };

    RtlZeroMemory(&ext->config, sizeof(ext->config));

    for (ULONG i = 0; i < ARRAYSIZE(default_processes) && i < ENCRYPTIC_MAX_BLOCKED_NAMES; ++i) {
        RtlStringCchCopyW(ext->config.blocked_processes[i], ENCRYPTIC_BLOCKED_NAME_CHARS, default_processes[i]);
        ++ext->config.blocked_process_count;
    }
    for (ULONG i = 0; i < ARRAYSIZE(default_drivers) && i < ENCRYPTIC_MAX_BLOCKED_NAMES; ++i) {
        RtlStringCchCopyW(ext->config.blocked_drivers[i], ENCRYPTIC_BLOCKED_NAME_CHARS, default_drivers[i]);
        ++ext->config.blocked_driver_count;
    }
    for (ULONG i = 0; i < ARRAYSIZE(default_modules) && i < ENCRYPTIC_MAX_BLOCKED_NAMES; ++i) {
        RtlStringCchCopyW(ext->config.blocked_modules[i], ENCRYPTIC_BLOCKED_NAME_CHARS, default_modules[i]);
        ++ext->config.blocked_module_count;
    }
}

static VOID ProcessNotifyRoutineEx(_Inout_ PEPROCESS Process,
                                   _In_ HANDLE ProcessId,
                                   _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    UNREFERENCED_PARAMETER(Process);

    if (!g_ext) {
        return;
    }

    /* Protected game exited — release session so we don't affect other titles */
    if (!CreateInfo) {
        if (ProcessId == g_ext->target_pid) {
            EncrypticClearSession(L"protected process exited");
        }
        return;
    }

    if (!EncrypticSessionActive() || !CreateInfo->ImageFileName) {
        return;
    }

    /* Global cheat blocking is OPT-IN — default off to avoid interfering with other games/tools */
    if ((g_ext->target_flags & ENCRYPTIC_TARGET_BLOCK_GLOBAL_CHEATS) &&
        EncrypticMatchesBlocklist(CreateInfo->ImageFileName, FALSE)) {
        InterlockedIncrement(&g_ext->processes_blocked);
        EncrypticPushEvent(EncrypticKernelBlockedProcess, ProcessId, PsGetCurrentProcessId(),
                           CreateInfo->ImageFileName->Buffer);
        CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
    }
}

static VOID LoadImageNotifyRoutine(_In_opt_ PUNICODE_STRING FullImageName,
                                   _In_ HANDLE ProcessId,
                                   _In_ PIMAGE_INFO ImageInfo)
{
    UNREFERENCED_PARAMETER(ImageInfo);

    if (!g_ext || !FullImageName || !FullImageName->Buffer || !EncrypticSessionActive()) {
        return;
    }

    const HANDLE target = g_ext->target_pid;

    /* Only inspect modules/drivers relevant to OUR protected game process */
    if (ProcessId != target) {
        return;
    }

    if ((g_ext->target_flags & ENCRYPTIC_TARGET_BLOCK_DRIVERS) &&
        EncrypticMatchesBlocklist(FullImageName, TRUE)) {
        InterlockedIncrement(&g_ext->images_blocked);
        EncrypticPushEvent(EncrypticKernelBlockedDriver, target, PsGetCurrentProcessId(),
                           FullImageName->Buffer);
        return;
    }

    if ((g_ext->target_flags & ENCRYPTIC_TARGET_BLOCK_MODULES) &&
        EncrypticMatchesModuleBlocklist(FullImageName)) {
        InterlockedIncrement(&g_ext->images_blocked);
        EncrypticPushEvent(EncrypticKernelSuspiciousImage, target, PsGetCurrentProcessId(),
                           FullImageName->Buffer);
    }
}

static OB_PREOP_CALLBACK_STATUS ObPreOperationCallback(_In_ PVOID RegistrationContext,
                                                       _Inout_ POB_PRE_OPERATION_INFORMATION Info)
{
    UNREFERENCED_PARAMETER(RegistrationContext);

    if (!EncrypticSessionActive() || !(g_ext->target_flags & ENCRYPTIC_TARGET_PROTECT_HANDLES)) {
        return OB_PREOP_SUCCESS;
    }

    if (Info->ObjectType != *PsProcessType) {
        return OB_PREOP_SUCCESS;
    }

    PEPROCESS target_process = (PEPROCESS)Info->Object;
    const HANDLE target_pid = PsGetProcessId(target_process);
    if (target_pid != g_ext->target_pid) {
        return OB_PREOP_SUCCESS;
    }

    const HANDLE caller = PsGetCurrentProcessId();
    if (caller == g_ext->target_pid || caller == g_ext->client_pid) {
        return OB_PREOP_SUCCESS;
    }

    /* Never interfere with other anti-cheat, Steam, Roblox, or system processes */
    if (EncrypticIsTrustedProcess(caller)) {
        return OB_PREOP_SUCCESS;
    }

    ACCESS_MASK strip = PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                          PROCESS_CREATE_THREAD | PROCESS_DUP_HANDLE;

    ACCESS_MASK desired = 0;
    if (Info->Operation == OB_OPERATION_HANDLE_CREATE) {
        desired = Info->Parameters->CreateHandleInformation.DesiredAccess;
        Info->Parameters->CreateHandleInformation.DesiredAccess &= ~strip;
    } else if (Info->Operation == OB_OPERATION_HANDLE_DUPLICATE) {
        desired = Info->Parameters->DuplicateHandleInformation.DesiredAccess;
        Info->Parameters->DuplicateHandleInformation.DesiredAccess &= ~strip;
    }

    if (desired & strip) {
        InterlockedIncrement(&g_ext->handles_stripped);
        EncrypticPushEvent(EncrypticKernelHandleStripped, target_pid, caller, L"external memory access blocked");
    }

    return OB_PREOP_SUCCESS;
}

static NTSTATUS EncrypticRegisterCallbacks(_Inout_ PENCRYPTIC_DEVICE_EXTENSION ext)
{
    NTSTATUS status;

    status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, FALSE);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    ext->process_notify_handle = (PVOID)1;

    status = PsSetLoadImageNotifyRoutine(LoadImageNotifyRoutine);
    if (!NT_SUCCESS(status)) {
        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, TRUE);
        ext->process_notify_handle = NULL;
        return status;
    }
    ext->image_notify_handle = (PVOID)1;

    /* Always register OB callbacks; they no-op unless a scoped session is active */
    {
        OB_CALLBACK_REGISTRATION ob_reg;
        OB_OPERATION_REGISTRATION op_reg;
        UNICODE_STRING altitude;

        RtlInitUnicodeString(&altitude, L"321000.5");

        RtlZeroMemory(&op_reg, sizeof(op_reg));
        op_reg.ObjectType = PsProcessType;
        op_reg.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
        op_reg.PreOperation = ObPreOperationCallback;

        RtlZeroMemory(&ob_reg, sizeof(ob_reg));
        ob_reg.Version = OB_CALLBACK_REGISTRATION_VERSION;
        ob_reg.OperationRegistrationCount = 1;
        ob_reg.Altitude = altitude;
        ob_reg.RegistrationContext = ext;
        ob_reg.OperationRegistration = &op_reg;

        status = ObRegisterCallbacks(&ob_reg, &ext->ob_callback_handle);
        if (!NT_SUCCESS(status)) {
            ext->ob_callback_handle = NULL;
        }
    }

    return STATUS_SUCCESS;
}

static VOID EncrypticUnregisterCallbacks(_Inout_ PENCRYPTIC_DEVICE_EXTENSION ext)
{
    if (ext->ob_callback_handle) {
        ObUnRegisterCallbacks(ext->ob_callback_handle);
        ext->ob_callback_handle = NULL;
    }
    if (ext->image_notify_handle) {
        PsRemoveLoadImageNotifyRoutine(LoadImageNotifyRoutine);
        ext->image_notify_handle = NULL;
    }
    if (ext->process_notify_handle) {
        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, TRUE);
        ext->process_notify_handle = NULL;
    }
}

static NTSTATUS EncrypticHandleGetVersion(_Out_ PENCRYPTIC_KERNEL_VER_INFO out)
{
    out->driver_version = ENCRYPTIC_KERNEL_VERSION;
    out->feature_flags = ENCRYPTIC_FEATURE_PROCESS_NOTIFY | ENCRYPTIC_FEATURE_IMAGE_NOTIFY |
                           ENCRYPTIC_FEATURE_OB_CALLBACKS | ENCRYPTIC_FEATURE_SESSION_SCOPED |
                           ENCRYPTIC_FEATURE_TRUSTED_WHITELIST;
    return STATUS_SUCCESS;
}

static NTSTATUS EncrypticHandleSetTarget(_In_ PENCRYPTIC_KERNEL_TARGET* in)
{
    if (!g_ext || !in || in->process_id == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (in->session_token == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    g_ext->target_pid = UlongToHandle(in->process_id);
    g_ext->client_pid = UlongToHandle(in->client_pid ? in->client_pid : in->process_id);
    g_ext->target_flags = in->flags;
    g_ext->session_token = in->session_token;
    g_ext->session_active = 1;

    EncrypticPushEvent(EncrypticKernelSessionStarted, g_ext->target_pid, g_ext->client_pid,
                       L"scoped session started");
    return STATUS_SUCCESS;
}

static NTSTATUS EncrypticHandleClearTarget(_In_ PENCRYPTIC_KERNEL_SESSION* in)
{
    if (!g_ext || !in) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ext->session_active) {
        return STATUS_SUCCESS;
    }

    if (in->session_token != 0 && in->session_token != g_ext->session_token) {
        return STATUS_ACCESS_DENIED;
    }

    EncrypticClearSession(L"session cleared by client");
    return STATUS_SUCCESS;
}

static NTSTATUS EncrypticHandleUpdateConfig(_In_ PENCRYPTIC_KERNEL_CONFIG* in)
{
    if (!g_ext || !in) {
        return STATUS_INVALID_PARAMETER;
    }

    KIRQL old_irql;
    KeAcquireSpinLock(&g_ext->config_lock, &old_irql);

    if (in->blocked_process_count > ENCRYPTIC_MAX_BLOCKED_NAMES) {
        in->blocked_process_count = ENCRYPTIC_MAX_BLOCKED_NAMES;
    }
    if (in->blocked_driver_count > ENCRYPTIC_MAX_BLOCKED_NAMES) {
        in->blocked_driver_count = ENCRYPTIC_MAX_BLOCKED_NAMES;
    }
    if (in->blocked_module_count > ENCRYPTIC_MAX_BLOCKED_NAMES) {
        in->blocked_module_count = ENCRYPTIC_MAX_BLOCKED_NAMES;
    }

    RtlCopyMemory(&g_ext->config, in, sizeof(ENCRYPTIC_KERNEL_CONFIG));
    KeReleaseSpinLock(&g_ext->config_lock, old_irql);
    return STATUS_SUCCESS;
}

static NTSTATUS EncrypticHandlePollEvents(_Out_ PENCRYPTIC_KERNEL_EVENT_BATCH* out)
{
    if (!g_ext || !out) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(out, sizeof(*out));

    KIRQL old_irql;
    KeAcquireSpinLock(&g_ext->event_lock, &old_irql);

    while (g_ext->event_tail != g_ext->event_head && out->count < ENCRYPTIC_MAX_EVENTS_PER_POLL) {
        out->events[out->count++] = g_ext->event_ring[g_ext->event_tail];
        g_ext->event_tail = (g_ext->event_tail + 1) % ENCRYPTIC_EVENT_RING_SIZE;
    }

    KeReleaseSpinLock(&g_ext->event_lock, old_irql);
    return STATUS_SUCCESS;
}

static NTSTATUS EncrypticHandleGetStatus(_Out_ PENCRYPTIC_KERNEL_STATUS* out)
{
    if (!g_ext || !out) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(out, sizeof(*out));
    out->driver_active = 1;
    out->session_active = g_ext->session_active ? 1u : 0u;
    out->target_pid = HandleToUlong(g_ext->target_pid);
    out->client_pid = HandleToUlong(g_ext->client_pid);
    out->target_flags = g_ext->target_flags;
    out->callbacks_registered = (g_ext->process_notify_handle && g_ext->image_notify_handle) ? 1 : 0;
    out->handles_stripped = (uint32_t)g_ext->handles_stripped;
    out->processes_blocked = (uint32_t)g_ext->processes_blocked;
    out->images_blocked = (uint32_t)g_ext->images_blocked;

    KIRQL old_irql;
    KeAcquireSpinLock(&g_ext->event_lock, &old_irql);
    if (g_ext->event_head >= g_ext->event_tail) {
        out->events_pending = (uint32_t)(g_ext->event_head - g_ext->event_tail);
    } else {
        out->events_pending = (uint32_t)(ENCRYPTIC_EVENT_RING_SIZE - g_ext->event_tail + g_ext->event_head);
    }
    KeReleaseSpinLock(&g_ext->event_lock, old_irql);

    return STATUS_SUCCESS;
}

static NTSTATUS EncrypticDeviceControl(_In_ PDEVICE_OBJECT DeviceObject,
                                       _Inout_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG bytes = 0;

    const ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;
    const ULONG in_len = stack->Parameters.DeviceIoControl.InputBufferLength;
    const ULONG out_len = stack->Parameters.DeviceIoControl.OutputBufferLength;

    switch (code) {
    case IOCTL_ENCRYPTIC_GET_VERSION:
        if (buffer && out_len >= sizeof(ENCRYPTIC_KERNEL_VER_INFO)) {
            status = EncrypticHandleGetVersion((PENCRYPTIC_KERNEL_VER_INFO)buffer);
            bytes = sizeof(ENCRYPTIC_KERNEL_VER_INFO);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_ENCRYPTIC_SET_TARGET:
        if (buffer && in_len >= sizeof(ENCRYPTIC_KERNEL_TARGET)) {
            status = EncrypticHandleSetTarget((PENCRYPTIC_KERNEL_TARGET)buffer);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_ENCRYPTIC_UPDATE_CONFIG:
        if (buffer && in_len >= sizeof(ENCRYPTIC_KERNEL_CONFIG)) {
            status = EncrypticHandleUpdateConfig((PENCRYPTIC_KERNEL_CONFIG)buffer);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_ENCRYPTIC_POLL_EVENTS:
        if (buffer && out_len >= sizeof(ENCRYPTIC_KERNEL_EVENT_BATCH)) {
            status = EncrypticHandlePollEvents((PENCRYPTIC_KERNEL_EVENT_BATCH)buffer);
            bytes = sizeof(ENCRYPTIC_KERNEL_EVENT_BATCH);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_ENCRYPTIC_GET_STATUS:
        if (buffer && out_len >= sizeof(ENCRYPTIC_KERNEL_STATUS)) {
            status = EncrypticHandleGetStatus((PENCRYPTIC_KERNEL_STATUS)buffer);
            bytes = sizeof(ENCRYPTIC_KERNEL_STATUS);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_ENCRYPTIC_CLEAR_TARGET:
        if (buffer && in_len >= sizeof(ENCRYPTIC_KERNEL_SESSION)) {
            status = EncrypticHandleClearTarget((PENCRYPTIC_KERNEL_SESSION)buffer);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytes;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS EncrypticCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static VOID EncrypticDriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    if (g_ext) {
        EncrypticUnregisterCallbacks(g_ext);

        UNICODE_STRING dos_name;
        RtlInitUnicodeString(&dos_name, ENCRYPTIC_KERNEL_DOS_DEVICE);
        IoDeleteSymbolicLink(&dos_name);

        if (g_ext->device_object) {
            IoDeleteDevice(g_ext->device_object);
        }

        ExFreePoolWithTag(g_ext, ENCRYPTIC_POOL_TAG);
        g_ext = NULL;
    }
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;

    PENCRYPTIC_DEVICE_EXTENSION ext = (PENCRYPTIC_DEVICE_EXTENSION)ExAllocatePoolWithTag(
        NonPagedPool, sizeof(ENCRYPTIC_DEVICE_EXTENSION), ENCRYPTIC_POOL_TAG);
    if (!ext) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(ext, sizeof(*ext));
    KeInitializeSpinLock(&ext->config_lock);
    KeInitializeSpinLock(&ext->event_lock);
    EncrypticLoadDefaultConfig(ext);
    g_ext = ext;

    RtlInitUnicodeString(&ext->device_name, ENCRYPTIC_KERNEL_DEVICE_PATH);
    RtlInitUnicodeString(&ext->dos_device_name, ENCRYPTIC_KERNEL_DOS_DEVICE);

    status = IoCreateDevice(DriverObject, 0, &ext->device_name, FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN, FALSE, &ext->device_object);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(ext, ENCRYPTIC_POOL_TAG);
        g_ext = NULL;
        return status;
    }

    status = IoCreateSymbolicLink(&ext->dos_device_name, &ext->device_name);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(ext->device_object);
        ExFreePoolWithTag(ext, ENCRYPTIC_POOL_TAG);
        g_ext = NULL;
        return status;
    }

    ext->device_object->Flags |= DO_BUFFERED_IO;
    ext->device_object->Flags &= ~DO_DEVICE_INITIALIZING;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = EncrypticCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = EncrypticCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = EncrypticDeviceControl;
    DriverObject->DriverUnload = EncrypticDriverUnload;

    status = EncrypticRegisterCallbacks(ext);
    if (!NT_SUCCESS(status)) {
        IoDeleteSymbolicLink(&ext->dos_device_name);
        IoDeleteDevice(ext->device_object);
        ExFreePoolWithTag(ext, ENCRYPTIC_POOL_TAG);
        g_ext = NULL;
        return status;
    }

    return STATUS_SUCCESS;
}
