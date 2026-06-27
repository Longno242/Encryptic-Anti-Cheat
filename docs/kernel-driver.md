# Encryptic Kernel Driver (EncrypticGuard.sys)

EncrypticGuard is a **scoped** kernel-mode companion driver. It only protects **your registered game process** and avoids interfering with other games, Steam, Roblox, or third-party anti-cheat.

## Isolation guarantees

| Behavior | Scoped? | Notes |
|----------|---------|-------|
| Handle protection (ObRegisterCallbacks) | **Yes** | Only your registered PID |
| Module/driver scan | **Yes** | Only images loaded into your game |
| Cheat process blocking | **Opt-in** | `block_global_cheats: false` by default |
| Trusted whitelist | **Yes** | EAC, BattlEye, Vanguard, Steam, Roblox, System — never stripped |
| Session cleanup | **Yes** | Auto-clears when your game exits |
| Session token | **Yes** | Prevents other processes hijacking the driver |

### Trusted processes (never blocked)

EasyAntiCheat, BattlEye, Vanguard, FaceIt, Steam, RobloxPlayerBeta, System, csrss, Defender, explorer, etc.

## What the driver does

| Feature | Description |
|---------|-------------|
| **Scoped handle protection** | Strips external memory access to **your game only** |
| **Module load notify** | Detects cheat DLLs loading into **your game only** |
| **Optional global cheat block** | OFF by default — enable only if you accept system-wide blocking |
| **Session lifecycle** | Starts on connect, clears on game exit or disconnect |
| **Event queue** | User-mode polls violations via IOCTL |

## Requirements

1. Windows 10/11 x64
2. Windows Driver Kit (WDK)
3. Administrator + test signing (dev) or EV signing (production)

## Quick start

```powershell
powershell -ExecutionPolicy Bypass -File setup/Install-KernelDriver.ps1 -EnableTestSigning
# reboot
powershell -ExecutionPolicy Bypass -File setup/Install-KernelDriver.ps1
```

## Config (scoped — safe default)

```json
{
  "guards": { "kernel_guard": true },
  "kernel": {
    "protect_handles": true,
    "block_drivers": true,
    "block_modules": true,
    "block_global_cheats": false
  }
}
```

Set `block_global_cheats: true` only if you want to block cheat tools system-wide while your game runs (may affect dev tools / other titles).

## Architecture

```
Your Game (PID registered)
  └── encryptic_core.dll
        └── KernelGuard → session token + IOCTL
              └── EncrypticGuard.sys
                    ├── ObRegisterCallbacks (target PID only)
                    ├── PsSetLoadImageNotifyRoutine (target PID only)
                    └── PsSetCreateProcessNotifyRoutineEx (opt-in global block)
```

## Uninstall

```powershell
sc stop EncrypticGuard
sc delete EncrypticGuard
del C:\Windows\System32\drivers\EncrypticGuard.sys
```

## Production notes

- Test in a VM first — kernel bugs can BSOD
- Test signing is dev-only; production needs Microsoft attestation
- Pair with server-side validation (Roblox Luau guards or your game backend)
