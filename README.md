# Encryptic Anti-Cheat

Client-side anti-cheat for Windows games. One C++ core, thin wrappers per engine.

Works with Unity (IL2CPP + Mono), Unreal, Godot, native C++, and there's a Roblox server-side folder for Luau checks.

## Quick start

**New project?** Run `setup.bat`. It builds the DLL, asks a few questions, and drops a ready-to-copy folder under `dist/`.

**Just want to poke at it?** Run `play_test_game.bat` — small test game, F1–F6 trigger cheats, detections show in-game and can kick you out with a reason screen.

**Watermark preview:** `test.bat` then edit `encryptic_watermark.json`.

## Build

```powershell
cmake -B build -DENCRYPTIC_BUILD_SHARED=ON -DENCRYPTIC_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

Outputs `encryptic_core.dll` and `encryptic_core_static.lib` under `build/core/Release/`.

## Config

Two JSON files:

- `encryptic_config.json` — guards, telemetry, heartbeat, integrity
- `encryptic_watermark.json` — launch splash only

Load config at runtime:

```c
encryptic_start_from_file("encryptic_config.json");
encryptic_tick();
```

Or from C++:

```cpp
#include <anticheat/anticheat.hpp>

auto cfg = anticheat::Config::from_preset(anticheat::ProtectionPreset::Balanced);
cfg.watermark.game_title = "My Game";
anticheat::Encryptic::instance().start(cfg);
```

Presets: `Minimal`, `Balanced`, `Aggressive`. Toggle individual guards in the JSON or `Config` struct.

## What's in the box

| Guard | Does |
|-------|------|
| DllGuard | Module scan, optional LoadLibrary hook |
| DebuggerGuard | Debugger / breakpoint checks |
| HookGuard | Kernel32/ntdll hook detection |
| MemoryGuard | Checksum regions you register |
| ThreadGuard | Weird thread start addresses |
| TimingGuard | QPC vs GetTickCount drift |
| ProcessGuard | Blocked cheat tool process names |
| InputGuard | SendInput-style mismatches |
| Telemetry | POST violations to your API |
| Heartbeat | Session pings with miss threshold |
| IntegrityGuard | CRC on the core DLL + file manifest |
| WatchdogGuard | Background canary thread |
| Watermark | EAC-style launch card |

Full list and options: [docs/features.md](docs/features.md).

## Engine folders

```
engines/unity-il2cpp/
engines/unity-mono/
engines/unreal-engine/EncrypticAntiCheat/
engines/godot/
engines/native/
engines/roblox/          # server Luau only
```

## Telemetry (dev)

```bash
node server/examples/node/server.js
```

Listens on `8787` by default. Point `encryptic_config.json` at `http://127.0.0.1:8787/v1/violations`. `play_test_game.bat` starts this for you if nothing's already on that port.

Server-side validation notes: [docs/server-validation.md](docs/server-validation.md).

## Repo layout

```
core/                 C++ library
engines/              per-engine glue
examples/test_game/   Windows test harness
setup.bat             interactive setup wizard
docs/
server/examples/      Node + Python receivers
```

## Docs

- [Getting started](docs/getting-started.md)
- [Anti-bypass notes](docs/anti-bypass.md)
- [Contributing](CONTRIBUTING.md)

## License

MIT — see [LICENSE](LICENSE).
