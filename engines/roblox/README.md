# Roblox integration

Roblox **cannot** load native DLLs. Anti-cheat is **100% server-authoritative** Luau in `ServerScriptService`.

## Install

Copy `ServerScriptService/Encryptic/` into your experience, or import the all-in-one demo:

**`demo/EncrypticDemo.rbxmx`** — includes Encryptic, demo server script, and test GUI client.

## Quick start

```lua
local Encryptic = require(game.ServerScriptService.Encryptic.init)

Encryptic.start({
	maxSpeed = 48,
	maxTeleportDelta = 80,
	maxWalkSpeed = 24,
	maxFireRate = 12,
	maxRemotePerSecond = 40,
	maxStrikes = 5,
	enableFlyGuard = true,
	enableNoclipGuard = true,
	enableExploitGuard = true,
})
```

## Guards

| Guard | Detects |
|-------|---------|
| **MovementGuard** | Horizontal speed / velocity hacks |
| **TeleportGuard** | Teleport, large position deltas |
| **HumanoidGuard** | WalkSpeed, JumpPower, JumpHeight, HipHeight tamper |
| **FlyGuard** | PlatformStand fly, sustained mid-air hover |
| **NoclipGuard** | Character parts with `CanCollide` disabled |
| **FireRateGuard** | Shot rate limits (call `recordShot`) |
| **RemoteGuard** | RemoteEvent spam (call `track` or `wrapRemoteEvent`) |
| **ToolGuard** | Tool equip spam |
| **HitGuard** | Reach / hit distance exploits |
| **ExploitGuard** | Suspicious remote names |
| **BanManager** | Strike system → auto kick |

## Wire your game systems

```lua
-- Guns
if not Encryptic.FireRateGuard.recordShot(player) then return end

-- Remotes
Encryptic.RemoteGuard.wrapRemoteEvent(myRemote, function(player, data)
	-- handle
end)

-- Melee
if not Encryptic.HitGuard.validateHit(attacker, targetChar, hitPos) then return end

-- Tools
if not Encryptic.ToolGuard.trackEquip(player) then return end
```

## Studio demo

| File | Location in Studio |
|------|-------------------|
| `ServerScriptService/Encryptic/` | ModuleScripts (folder) |
| `demo/ServerScriptService/DemoGame.server.lua` | Script in ServerScriptService |
| `demo/StarterPlayer/.../DemoClient.client.lua` | LocalScript in StarterPlayerScripts |

Press **AC** (top-right) or **RightControl** in play mode to open the test panel. See [`demo/DEMO_SETUP.md`](demo/DEMO_SETUP.md).

Regenerate the rbxmx after editing sources:

```bash
python demo/build_studio_model.py
```

## PC companion (optional)

For a Windows Roblox companion or standalone port, use `encryptic_core.dll` with **scoped kernel mode** — it only protects your registered game process and whitelists Steam, Roblox, EAC, BattlEye, Vanguard, etc.

See [`docs/kernel-driver.md`](../../docs/kernel-driver.md).
