# Feature reference

## Watermark (launch splash)

**Like EAC**: compact centered card on a **fully transparent** background (desktop shows through — no black dim by default).

**Customize** via `encryptic_watermark.json` or `WatermarkConfig` in code:

| Field | Description |
|-------|-------------|
| `game_title` | Your game name on the card |
| `brand_title` / `brand_subtitle` | Branding (default Encryptic / Anti-Cheat) |
| `loading_stages` | Status messages that cycle during load |
| `footer_text` / `version_text` | Bottom line |
| `accent_color` | Progress bar + logo ring (muted blue default) |
| `fullscreen_dim` | Optional dim overlay (`false` by default) |
| `allow_skip` | Disabled by default — splash runs to completion |
| `display_duration_ms` | Load animation length |

**Preview**: run `test.bat` from the repo root.

**Config**: `watermark.enabled`, `watermark.show_on_launch`, `watermark.blocking`

---

## Protection presets

| Preset | Profile |
|--------|---------|
| `Minimal` | DLL + process + timing + watermark |
| `Balanced` | Default shipping profile |
| `Aggressive` | All guards + whitelist + signatures |
| `Custom` | Use your own toggles |

```cpp
auto cfg = anticheat::Config::from_preset(anticheat::ProtectionPreset::Aggressive);
```

---

## DllGuard

Anti DLL injection — module diff, optional `LoadLibrary` hook, Authenticode check.

**Config**: `dll_guard`, `block_unknown_dlls`, `allowed_modules`

---

## DebuggerGuard

`IsDebuggerPresent`, PEB, remote debugger, hardware breakpoints, `OutputDebugString` trap.

**Config**: `debugger_guard`, `terminate_on_debugger`

---

## HookGuard

IAT / inline hooks on `LoadLibrary`, `VirtualProtect`, `WriteProcessMemory`, etc.

**Config**: `hook_guard`, `hook_scan_interval_ms`

---

## MemoryGuard

CRC32 on registered regions; flags writable executable pages.

**Config**: `memory_guard`, `memory_scan_interval_ms`

---

## ModuleWhitelist

Only modules in `allowed_modules` may load after baseline.

**Config**: `module_whitelist`, `require_signature`

---

## ThreadGuard

Private executable thread start addresses; thread count growth.

**Config**: `thread_guard`, `thread_scan_interval_ms`

---

## TimingGuard

`QueryPerformanceCounter` vs `GetTickCount64` drift (speed hacks).

**Config**: `timing_guard`, `max_timing_drift_ms`

---

## HandleGuard

Suspicious external handles to your process.

**Config**: `handle_guard`, `handle_scan_interval_ms`

---

## ProcessGuard

Scans for known cheat tools (Cheat Engine, x64dbg, IDA, etc.). Extend via `blocked_processes`.

**Config**: `process_guard`, `process_scan_interval_ms`, `blocked_processes`

---

## VmGuard

Registry heuristics for VMware / VirtualBox.

**Config**: `vm_guard`

---

## OverlayGuard

Visible windows with suspicious class names (overlay injectors).

**Config**: `overlay_guard`, `overlay_scan_interval_ms`

---

## InputGuard

`GetAsyncKeyState` vs `GetKeyState` mismatch (synthetic input).

**Config**: `input_guard`, `input_scan_interval_ms`

---

## Global options

| Option | Description |
|--------|-------------|
| `kick_on_critical` | Your callback should end session on Critical |
| `log_violations` | Log all detections |
| `on_violation` | Custom handler for every detection |

---

## Limitations

Client-side checks can be bypassed. Use Encryptic for **defense in depth** + **telemetry**, and validate gameplay on your server.
