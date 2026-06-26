# Roblox integration

Roblox **cannot** load native DLLs like `encryptic_core.dll` into the client. Anti-cheat on Roblox is **server-authoritative** using Luau in `ServerScriptService`.

This folder provides:

- Server-side validation modules (movement, fire rate, remote spam)
- Client **signals only** (never trust for bans alone)
- Patterns that pair with Encryptic telemetry if you run a PC companion app outside Roblox

## Install

Copy `ServerScriptService/Encryptic/` into your Roblox experience.

## Server bootstrap

```lua
local Encryptic = require(script.Parent.Encryptic)
Encryptic.start({
    maxSpeed = 32,
    maxFireRate = 12, -- shots per second
})
```

## What to validate on server

| Check | Module |
|-------|--------|
| Speed / teleport | `MovementGuard.server.lua` |
| Fire rate | `FireRateGuard.server.lua` |
| Remote spam | `RemoteGuard.server.lua` |
| Hit claims | Your own raycast validation |

## PC games vs Roblox

For a **standalone** Roblox competitor or PC port, use `engines/unity-il2cpp`, `engines/native`, etc. with `encryptic_core.dll`.

## External telemetry (optional)

If you ship a Windows launcher with Encryptic native core, POST violations to your backend and correlate with Roblox `userId` session keys.
