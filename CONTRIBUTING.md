# Contributing to Encryptic Anti-Cheat

Thanks for helping improve Encryptic.

## Scope

Encryptic is a **user-mode client toolkit** plus **server validation docs**. We do not accept:

- Kernel drivers without maintainer discussion
- Features designed purely to harm users or bypass platform ToS
- Obfuscated malware-style payloads

## Development setup

```powershell
cmake -B build -DENCRYPTIC_BUILD_SHARED=ON -DENCRYPTIC_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

Run telemetry dev server:

```bash
node server/examples/node/server.js
```

Point `encryptic_config.json` telemetry URLs to `http://127.0.0.1:8787`.

## Pull requests

1. One logical change per PR when possible  
2. Update `docs/features.md` if you add guards or config keys  
3. Keep Windows builds green (GitHub Actions)  
4. Note false-positive risk for new heuristics  

## Code style

- Match existing C++17 patterns in `core/`  
- Prefer modular guards over monolithic checks  
- No single global `if (!enabled) return` that bypasses everything  

## Reporting security issues

See [SECURITY.md](SECURITY.md). Do not open public issues for exploit details.
