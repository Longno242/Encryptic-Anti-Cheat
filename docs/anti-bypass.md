# Anti-bypass design

Encryptic avoids a **single kill switch** (`if (!enabled) return`) that cheaters patch once.

## Distributed guards

Each module runs independently. Disabling one path should not disable:

- Debugger process **and** service **and** NT **and** window scans  
- DLL list diff **and** `LoadLibrary` hook  
- Self-integrity CRC **and** watchdog thread **and** periodic hook scan  

## Watchdog thread

A background thread verifies:

- Main loop calls `encryptic_tick()` (ping timestamps)  
- Canary value mutates each tick (not a static bool)  

## Self-integrity

CRC32 of `encryptic_core.dll` `.text` section at startup; re-checked on interval and on anti-tamper ticks.

## Server correlation

Client bypass does not help if:

- Heartbeats stop  
- Violations POST to your API  
- Gameplay is validated server-side  

## Honest limits

User-mode only. Kernel cheats and manual mapping can evade some checks. Raise cost; validate on server.
