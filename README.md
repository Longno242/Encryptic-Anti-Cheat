# Encryptic Anti-Cheat

Modular anti-cheat for **Roblox** (server Luau) and **Windows** games (C++ core + engine wrappers).

Built for developers who want real detection hooks—not just config files—with a Studio-ready demo you can import and test in minutes.

---

## Roblox (server-side) — start here

Roblox experiences **cannot** load native DLLs. Encryptic runs **100% on the server** in `ServerScriptService` and validates movement, remotes, combat, and exploit patterns authoritatively.

### Quick install (Studio)

1. Import **`engines/roblox/demo/EncrypticDemo.rbxmx`** (File → Import, or drag into Explorer).
2. Move instances:
   - **`Encryptic`** folder → `ServerScriptService`
   - **`DemoGame`** → `ServerScriptService`
   - **`DemoClient`** → `StarterPlayer → StarterPlayerScripts`
3. Press **Play**. Open the test panel with **AC** (top-right) or **RightControl**.
4. Watch **View → Output → Server** for `[Encryptic]` violation lines.

Full walkthrough: [`engines/roblox/demo/DEMO_SETUP.md`](engines/roblox/demo/DEMO_SETUP.md)

### Use in your own game

Copy `engines/roblox/ServerScriptService/Encryptic/` into your place, then:

```lua
local Encryptic = require(game.ServerScriptService.Encryptic.init)

Encryptic.start({
	maxSpeed = 48,
	maxWalkSpeed = 24,
	maxFireRate = 12,
	maxRemotePerSecond = 40,
	maxStrikes = 5,
	enableFlyGuard = true,
	enableNoclipGuard = true,
	enableExploitGuard = true,
})
```

Wire your systems:

```lua
-- Guns
if not Encryptic.FireRateGuard.recordShot(player) then return end

-- Remotes
Encryptic.RemoteGuard.wrapRemoteEvent(myRemote, function(player, ...)
	-- your handler
end)

-- Melee reach
if not Encryptic.HitGuard.validateHit(attacker, targetChar, hitPos) then return end
```

### Roblox guards

| Guard | Detects |
|-------|---------|
| **MovementGuard** | Horizontal speed / velocity hacks |
| **TeleportGuard** | Large position jumps (teleport) |
| **HumanoidGuard** | WalkSpeed, JumpPower, JumpHeight, HipHeight tamper |
| **FlyGuard** | PlatformStand fly, mid-air hover |
| **NoclipGuard** | Character parts with collision disabled |
| **FireRateGuard** | Shot rate limits |
| **RemoteGuard** | RemoteEvent spam |
| **ToolGuard** | Tool equip spam |
| **HitGuard** | Hit distance / reach exploits |
| **ExploitGuard** | Suspicious remote names |
| **BanManager** | Strike system → warn / kick |

More detail: [`engines/roblox/README.md`](engines/roblox/README.md)

---

## Windows client (C++ core)

One shared library, thin wrappers per engine: Unity (IL2CPP + Mono), Unreal, Godot, native C++.

### Quick start

**New project?** Run `setup.bat` — builds the DLL, walks through options, outputs a copy-ready folder under `dist/`.

**Try detections locally:** `play_test_game.bat` — F1–F6 simulate cheats; violations show in-game and can kick with a reason screen.

**Watermark preview:** `test.bat` + edit `encryptic_watermark.json`.

### Build

```powershell
cmake -B build -DENCRYPTIC_BUILD_SHARED=ON -DENCRYPTIC_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

Output: `build/core/Release/encryptic_core.dll`

### Config

| File | Purpose |
|------|---------|
| `encryptic_config.json` | Guards, telemetry, heartbeat, integrity, kernel options |
| `encryptic_watermark.json` | Launch splash / watermark card |

```c
encryptic_start_from_file("encryptic_config.json");
encryptic_tick();
```

Presets: `Minimal`, `Balanced`, `Aggressive`. See [`docs/features.md`](docs/features.md).

### Windows guards (high level)

DllGuard · DebuggerGuard · HookGuard · MemoryGuard · ThreadGuard · TimingGuard · ProcessGuard · ThreatIntelGuard · InjectionGuard · AntiBypassGuard · HashGuard · PipeGuard · InputGuard · HeartbeatGuard · IntegrityGuard · WatchdogGuard · KernelGuard (optional driver)

Kernel driver docs (session-scoped, whitelists EAC/BattlEye/Steam/Roblox): [`docs/kernel-driver.md`](docs/kernel-driver.md)

---

## Repo layout

```
engines/roblox/              Roblox server Luau + Studio demo + EncrypticDemo.rbxmx
core/                        C++ anti-cheat library
engines/unity-il2cpp/        Unity IL2CPP wrapper
engines/unity-mono/          Unity Mono wrapper
engines/unreal-engine/       Unreal plugin
engines/godot/               Godot integration
engines/native/              Native C++ sample
examples/test_game/          Windows test harness
setup/                       Install scripts (incl. kernel driver)
docs/                        Feature + integration docs
server/examples/             Node + Python telemetry receivers
```

---

## Telemetry (development)

```bash
node server/examples/node/server.js
```

Default port `8787`. Point `encryptic_config.json` telemetry URL at `http://127.0.0.1:8787/v1/violations`.

---

## Documentation

- [Getting started](docs/getting-started.md)
- [Features](docs/features.md)
- [Server validation](docs/server-validation.md)
- [Anti-bypass notes](docs/anti-bypass.md)
- [Roblox integration](engines/roblox/README.md)
- [Contributing](CONTRIBUTING.md)

---

## License

MIT — see [LICENSE](LICENSE).
