# Encryptic Roblox Demo — Studio Setup

Follow these steps to see the anti-cheat working in Roblox Studio.

## 1. Copy files into your place

In **Roblox Studio**, open a blank Baseplate (or any place).

### ServerScriptService

1. Copy the entire folder:
   `engines/roblox/ServerScriptService/Encryptic`
   → into **ServerScriptService** as a folder named **Encryptic**

2. Copy:
   `engines/roblox/demo/ServerScriptService/DemoGame.server.lua`
   → into **ServerScriptService** (same level as Encryptic)

Your Explorer should look like:

```
ServerScriptService
├── Encryptic          (ModuleScript init + child modules)
│   ├── init
│   ├── BanManager
│   ├── MovementGuard
│   └── ... (all other guard modules)
└── DemoGame           (Script — rename file if Studio drops .server.lua)
```

> **Tip:** If you paste from this repo, `init.lua` must become a **ModuleScript** named `init` inside folder `Encryptic`. Other `.lua` files become **ModuleScripts** with matching names.

### StarterPlayer

Copy:
`engines/roblox/demo/StarterPlayer/StarterPlayerScripts/DemoClient.client.lua`
→ into **StarterPlayer → StarterPlayerScripts**

## 2. Enable Server output

View → Output (so you can see violation messages).

## 3. Play test

Press **Play** (F5).

You should see:

```
[Encryptic] Advanced server guards active
[Demo] Encryptic started...
[Demo Client] F = hold to spam shots | G = spam coin remote
```

## 4. Try these tests

| Action | What triggers | What you see |
|--------|----------------|--------------|
| Hold **F** | **FireRateGuard** | Output: `Fire rate exploit...` then strikes increase |
| Press **G** | **RemoteGuard** | Output: `Remote spam...` |
| In Studio command bar (server): set WalkSpeed on your Humanoid to 100 | **HumanoidGuard** | WalkSpeed clamped + violation logged |
| Fly in air > ~2.5s without landing | **FlyGuard** | Violation + forced downward velocity |
| Move character very far in one frame (teleport exploit) | **TeleportGuard** | Position rolled back |

After **3 strike points** (demo config), the player is **kicked** with an Encryptic message.

## 5. How it works (simple)

```
Client                    Server
  |                         |
  |-- FireWeapon --------->|  RemoteGuard.wrapRemoteEvent
  |                         |  FireRateGuard.recordShot()
  |                         |  BanManager.record() on fail
  |                         |
  |<-- kick if strikes ----|  player:Kick()
```

Movement/fly/humanoid guards run automatically on **Heartbeat** — no extra code needed.

## 6. Wire into your real game

Replace demo remotes with your game's remotes:

```lua
local Encryptic = require(game.ServerScriptService.Encryptic.init)
Encryptic.start({ maxStrikes = 5 })

-- Your gun remote
Encryptic.RemoteGuard.wrapRemoteEvent(game.ReplicatedStorage.Shoot, function(player, ...)
    if not Encryptic.FireRateGuard.recordShot(player) then return end
    -- your damage logic
end)
```

See `engines/roblox/README.md` for the full guard list.
