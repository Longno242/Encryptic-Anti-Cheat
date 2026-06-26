# Server-side validation

Encryptic client guards raise the bar. **Your server decides what is allowed.**

Production anti-cheat (EAC, BattlEye, VAC) relies on:

1. **Client telemetry** — detections, heartbeats, session tokens  
2. **Authoritative server** — gameplay state the client cannot own  
3. **Backend correlation** — ban / kick on repeated signals  

## Rules of thumb

| Never trust client for | Server must own |
|------------------------|-----------------|
| Position / velocity | Movement simulation + caps |
| Damage / kills | Hit validation + line-of-sight |
| Inventory / currency | Database transactions |
| Fire rate / cooldowns | Rate limits per action |
| Match outcome | Replay or stat sanity checks |

## Movement caps (example)

```lua
-- Roblox / Luau style
local MAX_SPEED = 32
game:GetService("RunService").Heartbeat:Connect(function()
    for _, player in Players:GetPlayers() do
        local char = player.Character
        local hrp = char and char:FindFirstChild("HumanoidRootPart")
        if hrp and hrp.AssemblyLinearVelocity.Magnitude > MAX_SPEED then
            -- flag, rollback, or kick
        end
    end
end)
```

```cpp
// Native / UE — server tick
if (FVector::Dist(OldPos, NewPos) / DeltaTime > MaxSpeed) {
    RejectMove(Player);
}
```

## Fire rate validation

```json
{
  "weapon_id": "rifle",
  "min_interval_ms": 95,
  "max_burst": 3
}
```

Server records `last_fire_time` per player. Reject shots that arrive faster than `min_interval_ms`.

## Inventory authority

- Client sends **intent** (`"use_item", id`)  
- Server checks ownership, cooldown, quantity  
- Server broadcasts result  

Never apply item effects from client-only scripts.

## Replay sanity

Log compact event stream per match:

```
t=1200 move (12.4, 0, 88.1)
t=1250 fire weapon=rifle target=player_9
t=1300 damage dealt=34
```

Offline jobs flag impossible sequences (teleport, infinite ammo).

## Encryptic telemetry integration

Point `encryptic_config.json` at your API:

```json
"telemetry": {
  "enabled": true,
  "endpoint_url": "https://api.yourgame.com/v1/violations",
  "api_key": "server-key"
}
```

Payload shape:

```json
{
  "session_id": "enc_a1b2...",
  "timestamp_ms": 1710000000000,
  "module_hash": "a3f9c21",
  "type": "debugger",
  "severity": 3,
  "message": "Debugger detected",
  "detail": "NtQueryInformationProcess debug port/object"
}
```

## Heartbeat

```json
"heartbeat": {
  "enabled": true,
  "endpoint_url": "https://api.yourgame.com/v1/heartbeat",
  "interval_ms": 30000,
  "miss_threshold": 2
}
```

Server should invalidate session token after missed heartbeats.

## Sample receivers

See `server/examples/` for local Node.js and Python servers you can run during development.
