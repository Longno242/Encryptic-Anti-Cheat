# Engine integrations

| Folder | Stack | Native DLL |
|--------|-------|------------|
| [unity-il2cpp](unity-il2cpp/) | Unity IL2CPP | `encryptic_core.dll` |
| [unity-mono](unity-mono/) | Unity Mono | Optional |
| [unreal-engine](unreal-engine/) | UE 5.x | `encryptic_core.dll` |
| [godot](godot/) | Godot 4 | When built |
| [native](native/) | Custom C++ | Link static or DLL |
| [roblox](roblox/) | Roblox Luau server | N/A (server-side) |

## Config

Use root **`encryptic_config.json`** for all guards + telemetry + heartbeat.

```c
encryptic_start_from_file("encryptic_config.json");
```

## Dev telemetry server

```bash
node server/examples/node/server.js
```

See [docs/server-validation.md](../docs/server-validation.md).
