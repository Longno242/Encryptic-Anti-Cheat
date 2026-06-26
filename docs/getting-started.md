# Getting started

## Setup wizard

```bat
setup.bat
```

Builds the core, asks for game name / engine / preset, writes `encryptic_config.json`, and packages `dist/YourGame/` with the DLL, configs, engine files, and `INSTALL.txt`. Optional Unity path copy.

If you enabled telemetry in the wizard, run `dist/YourGame/START_TELEMETRY.bat` and follow `INSTALL.txt`.

## Test game

```bat
play_test_game.bat
```

Windows mini-game with guards on. F1–F6 = built-in cheat triggers. Right panel logs detections; real hits close the game and show the kick screen.

## Watermark preview

```bat
test.bat
```

Edit `encryptic_watermark.json` in the repo root, run again.

## Pick an engine

| Folder | Runtime |
|--------|---------|
| `engines/unity-il2cpp` | IL2CPP + `encryptic_core.dll` |
| `engines/unity-mono` | Mono / Editor |
| `engines/unreal-engine` | UE 5.x plugin |
| `engines/godot` | Godot 4.x |
| `engines/native` | Plain C++ |
| `engines/roblox` | Server Luau |

## Build the core

```powershell
cmake -B build -DENCRYPTIC_BUILD_SHARED=ON
cmake --build build --config Release
```

- `build/core/Release/encryptic_core.dll`
- `build/core/Release/encryptic_core_static.lib`

## Minimal integration

```cpp
#include <anticheat/anticheat.hpp>

anticheat::Config cfg = anticheat::Config::from_preset(anticheat::ProtectionPreset::Balanced);
cfg.watermark.game_title = "My Game";

cfg.on_violation = [](const anticheat::Violation& v) {
    // log, kick, whatever
};

anticheat::Encryptic::instance().start(cfg);
// call tick() every frame
```

Watermark shows on `start()` when `watermark.enabled` and `show_on_launch` are true.

## Production-ish checklist

1. `Balanced` or `Aggressive` on client, `tick()` every frame  
2. `encryptic_config.json` beside the game or in StreamingAssets  
3. Don't rely on client-only checks — validate movement, fire rate, etc. on the server  
4. Wire telemetry to something you actually read

See [features.md](features.md) for every guard flag.
