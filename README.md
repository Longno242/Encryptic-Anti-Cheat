# Encryptic Anti-Cheat

Modular **Windows** anti-cheat: one C++ core with thin wrappers for Unity, Unreal, Godot, and native games.

> **Roblox developer?** Server-side Luau anti-cheat lives in its own repo:  
> **[github.com/Longno242/Encryptic-Roblox-Anti-Cheat](https://github.com/Longno242/Encryptic-Roblox-Anti-Cheat)**

---

## Quick start

**New project?** Run `setup.bat` — builds the DLL, walks through options, outputs a copy-ready folder under `dist/`.

**Try detections locally:** `play_test_game.bat` — F1–F6 simulate cheats; violations show in-game and can kick with a reason screen.

**Watermark preview:** `test.bat` + edit `encryptic_watermark.json`.

---

## Build

```powershell
cmake -B build -DENCRYPTIC_BUILD_SHARED=ON -DENCRYPTIC_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

Output: `build/core/Release/encryptic_core.dll`

---

## Config

| File | Purpose |
|------|---------|
| `encryptic_config.json` | Guards, telemetry, heartbeat, integrity, kernel options |
| `encryptic_watermark.json` | Launch splash / watermark card |

```c
encryptic_start_from_file("encryptic_config.json");
encryptic_tick();
```

Presets: `Minimal`, `Balanced`, `Aggressive`. See [`docs/features.md`](docs/features.md).

---

## Guards

| Guard | Does |
|-------|------|
| DllGuard | Module scan, optional LoadLibrary hook |
| DebuggerGuard | Debugger / breakpoint checks |
| HookGuard | Kernel32/ntdll hook detection |
| MemoryGuard | Checksum regions you register |
| ThreadGuard | Weird thread start addresses |
| TimingGuard | QPC vs GetTickCount drift |
| ProcessGuard | Blocked cheat tool process names |
| ThreatIntelGuard | CE modules, drivers, overlay patterns |
| InjectionGuard | Suspicious parent chain, temp DLL loads |
| AntiBypassGuard | Kernel debugger, hide-from-debugger |
| HashGuard | Known cheat binary SHA256 |
| PipeGuard | Named pipes / mutex patterns |
| InputGuard | SendInput-style mismatches |
| HeartbeatGuard | HMAC session pings |
| IntegrityGuard | CRC on core DLL + manifest |
| WatchdogGuard | Background canary thread |
| KernelGuard | Optional session-scoped driver |
| Watermark | EAC-style launch card |

Kernel driver (whitelists EAC, BattlEye, Steam, Roblox): [`docs/kernel-driver.md`](docs/kernel-driver.md)

---

## Engine folders

```
core/                        C++ anti-cheat library
engines/unity-il2cpp/        Unity IL2CPP wrapper
engines/unity-mono/          Unity Mono wrapper
engines/unreal-engine/       Unreal plugin
engines/godot/               Godot integration
engines/native/              Native C++ sample
engines/roblox/              → links to Roblox repo (Luau is separate)
examples/test_game/          Windows test harness
setup/                       Install scripts
docs/
server/examples/             Telemetry receivers
```

---

## Roblox

Roblox cannot load native DLLs. Use the dedicated server Luau package:

**[Encryptic-Roblox-Anti-Cheat](https://github.com/Longno242/Encryptic-Roblox-Anti-Cheat)** — guards, Studio demo, `EncrypticDemo.rbxmx`, test GUI.

---

## Telemetry (development)

```bash
node server/examples/node/server.js
```

Default port `8787`. Point `encryptic_config.json` at `http://127.0.0.1:8787/v1/violations`.

---

## Documentation

- [Getting started](docs/getting-started.md)
- [Features](docs/features.md)
- [Server validation](docs/server-validation.md)
- [Anti-bypass notes](docs/anti-bypass.md)
- [Contributing](CONTRIBUTING.md)

---

## License

MIT — see [LICENSE](LICENSE).
