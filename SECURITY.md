# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 1.x     | Yes       |

## Reporting a vulnerability

Email or DM the maintainer with:

- Description and impact  
- Reproduction steps  
- Whether it affects client-only bypass or server trust  

Please **do not** file public GitHub issues for bypass techniques.

## Scope disclaimer

Encryptic is a **user-mode** anti-cheat helper library. Determined attackers with kernel access or deep reverse engineering can bypass client checks. Document bypasses responsibly so we can improve distributed checks, integrity, and watchdog coverage — not claim impossible security.

## What we consider in scope

- Crashes or memory corruption in `encryptic_core`  
- Trivial single-byte disable of all protections  
- Telemetry/session handling flaws in sample servers  

## Out of scope

- Ban evasion as a service  
- Kernel cheat development  
- Roblox / platform ToS violations  

## Hardening philosophy

Protections are **distributed** across guards, watchdog threads, self-integrity CRC, and server validation. No single offset should disable the entire system.
