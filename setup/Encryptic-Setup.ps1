# Encryptic Anti-Cheat — interactive setup wizard
# Run via setup.bat from repo root

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Write-Banner {
    Write-Host ""
    Write-Host "  Encryptic Setup" -ForegroundColor Cyan
    Write-Host "  ---------------" -ForegroundColor DarkGray
}

function Test-Command($name) {
    return $null -ne (Get-Command $name -ErrorAction SilentlyContinue)
}

function Read-Default($prompt, $default) {
    $input = Read-Host "$prompt [$default]"
    if ([string]::IsNullOrWhiteSpace($input)) { return $default }
    return $input.Trim()
}

function Sanitize-Name($name) {
    return ($name -replace '[^\w\-]', '_').Trim('_')
}

Write-Banner

# --- Prerequisites ---
if (-not (Test-Command cmake)) {
    Write-Host "ERROR: CMake not found. Install CMake + Visual Studio C++ build tools." -ForegroundColor Red
    exit 1
}

$hasNode = Test-Command node
if (-not $hasNode) {
    Write-Host "NOTE: Node.js not found. Telemetry dev server will be skipped (copy server.js manually)." -ForegroundColor Yellow
}

# --- User input ---
$gameName = (Read-Default "Game title" "My Game") -replace '"', ''
Write-Host ""
Write-Host "Engine:" -ForegroundColor Yellow
Write-Host "  1) Unity IL2CPP (recommended for shipping PC)"
Write-Host "  2) Unity Mono"
Write-Host "  3) Unreal Engine 5"
Write-Host "  4) Godot 4"
Write-Host "  5) Native C++"
Write-Host "  6) Roblox (server-side Luau only)"
$engineChoice = Read-Default "Choose engine (1-6)" "1"

Write-Host ""
Write-Host "Protection preset:" -ForegroundColor Yellow
Write-Host "  1) balanced  - recommended for most games"
Write-Host "  2) aggressive - all guards + screenshots + file integrity"
Write-Host "  3) minimal    - lightweight"
$presetChoice = Read-Default "Choose preset (1-3)" "1"
$preset = switch ($presetChoice) {
    "2" { "aggressive" }
    "3" { "minimal" }
    default { "balanced" }
}

$enableTelemetry = (Read-Default "Enable telemetry + heartbeat to local dev server? (y/n)" "y").ToLower() -match '^y'
$telemetryPort = Read-Default "Telemetry server port" "8787"
$apiKey = Read-Default "API key (X-Encryptic-Key header)" "dev-key-change-me"

$gamePath = Read-Host "Optional: path to your game project (Enter to skip auto-copy)"
if ([string]::IsNullOrWhiteSpace($gamePath)) { $gamePath = $null }

# --- Build ---
Write-Host ""
Write-Host "[1/5] Building Encryptic core (Release)..." -ForegroundColor Green
cmake -B build -DENCRYPTIC_BUILD_SHARED=ON -DENCRYPTIC_BUILD_EXAMPLES=ON | Out-Null
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build build --config Release | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$dllSource = Join-Path $Root "build\core\Release\encryptic_core.dll"
if (-not (Test-Path $dllSource)) { throw "encryptic_core.dll not found after build" }

# --- Generate config ---
Write-Host "[2/5] Generating encryptic_config.json + watermark..." -ForegroundColor Green

$baseUrl = "http://127.0.0.1:$telemetryPort"
$telemetryEnabled = if ($enableTelemetry) { "true" } else { "false" }
$heartbeatEnabled = if ($enableTelemetry) { "true" } else { "false" }

$vmGuard = if ($preset -eq "aggressive") { "true" } else { "false" }
$moduleWhitelist = if ($preset -eq "aggressive") { "true" } else { "false" }
$screenshotOnCritical = if ($preset -eq "aggressive") { "true" } else { "true" }
$gameFileIntegrity = if ($preset -eq "aggressive") { "true" } else { "false" }

if ($preset -eq "minimal") {
    $guardsJson = @"
    "dll_guard": true,
    "debugger_guard": false,
    "hook_guard": false,
    "memory_guard": false,
    "module_whitelist": false,
    "thread_guard": false,
    "timing_guard": true,
    "handle_guard": false,
    "process_guard": true,
    "vm_guard": false,
    "overlay_guard": false,
    "input_guard": false,
    "watchdog_guard": true,
    "anti_tamper": true
"@
} else {
    $guardsJson = @"
    "dll_guard": true,
    "debugger_guard": true,
    "hook_guard": true,
    "memory_guard": true,
    "module_whitelist": $moduleWhitelist,
    "thread_guard": true,
    "timing_guard": true,
    "handle_guard": true,
    "process_guard": true,
    "vm_guard": $vmGuard,
    "overlay_guard": true,
    "input_guard": true,
    "watchdog_guard": true,
    "anti_tamper": true
"@
}

$configJson = @"
{
  "preset": "$preset",
  "session_id": "auto-generated-at-runtime",
  "guards": {
$guardsJson
  },
  "intervals_ms": {
    "hook_scan": 5000,
    "memory_scan": 3000,
    "thread_scan": 4000,
    "handle_scan": 10000,
    "timing_check": 1000,
    "process_scan": 8000,
    "overlay_scan": 6000,
    "input_scan": 2000,
    "watchdog": 2000,
    "integrity_scan": 15000
  },
  "blocked_processes": [
    "cheatengine", "x64dbg", "x32dbg", "ida64", "ida", "processhacker",
    "reclass", "httpdebugger", "ollydbg", "windbg", "immunitydebugger"
  ],
  "debugger_blocked_services": [
    "KProcessHacker3", "KSystemInformer", "HttpDebuggerPro", "x64dbg"
  ],
  "allowed_modules": [
    "encryptic_core.dll",
    "UnityPlayer.dll",
    "GameAssembly.dll",
    "steam_api64.dll"
  ],
  "overlay_whitelist": [
    "Steam", "Discord", "Xbox", "GameBar", "NVIDIA", "GeForce"
  ],
  "telemetry": {
    "enabled": $telemetryEnabled,
    "endpoint_url": "$baseUrl/v1/violations",
    "api_key": "$apiKey",
    "flush_interval_ms": 5000
  },
  "heartbeat": {
    "enabled": $heartbeatEnabled,
    "endpoint_url": "$baseUrl/v1/heartbeat",
    "session_token": "",
    "hmac_secret": "$apiKey",
    "interval_ms": 30000,
    "miss_threshold": 2
  },
  "integrity": {
    "self_integrity": true,
    "game_file_integrity": $gameFileIntegrity,
    "file_manifest": []
  },
  "screenshot": {
    "on_critical": $screenshotOnCritical,
    "output_dir": "encryptic_captures"
  },
  "watermark": {
    "enabled": true,
    "show_on_launch": true,
    "game_title": "$gameName"
  }
}
"@

$watermarkJson = @"
{
  "enabled": true,
  "show_on_launch": true,
  "fullscreen_dim": false,
  "allow_skip": false,
  "game_title": "$gameName",
  "brand_title": "Encryptic",
  "brand_subtitle": "Anti-Cheat",
  "status_text": "Starting protection service",
  "footer_text": "Protected by Encryptic Anti-Cheat",
  "version_text": "1.0.0",
  "display_duration_ms": 5200,
  "accent_color": [74, 158, 255],
  "card_color": [24, 25, 28]
}
"@

$configJson | Set-Content -Path (Join-Path $Root "encryptic_config.json") -Encoding UTF8
$watermarkJson | Set-Content -Path (Join-Path $Root "encryptic_watermark.json") -Encoding UTF8

# --- Dist package ---
$distName = Sanitize-Name $gameName
$distDir = Join-Path $Root "dist\$distName"
Write-Host "[3/5] Creating package: dist\$distName" -ForegroundColor Green

if (Test-Path $distDir) { Remove-Item $distDir -Recurse -Force }
New-Item -ItemType Directory -Path $distDir -Force | Out-Null

Copy-Item $dllSource (Join-Path $distDir "encryptic_core.dll")
$configJson | Set-Content (Join-Path $distDir "encryptic_config.json") -Encoding UTF8
$watermarkJson | Set-Content (Join-Path $distDir "encryptic_watermark.json") -Encoding UTF8

# Engine-specific bundle
$engineFolder = Join-Path $distDir "engine"
New-Item -ItemType Directory -Path $engineFolder -Force | Out-Null

switch ($engineChoice) {
    "2" {
        Copy-Item -Recurse (Join-Path $Root "engines\unity-mono\Runtime") (Join-Path $engineFolder "Encryptic") -Force
        New-Item -ItemType Directory -Path (Join-Path $engineFolder "Plugins\x86_64") -Force | Out-Null
        Copy-Item $dllSource (Join-Path $engineFolder "Plugins\x86_64\encryptic_core.dll")
        $engineLabel = "Unity Mono"
        $installSteps = @"
Copy 'engine/' into your Unity project Assets/ folder.

In code (first scene):
  Encryptic.AntiCheat.EncrypticAntiCheat.StartFromConfig();

Place encryptic_config.json in StreamingAssets/encryptic_config.json
"@
    }
    "3" {
        Copy-Item -Recurse (Join-Path $Root "engines\unreal-engine\EncrypticAntiCheat") $engineFolder -Force
        $ueDllDir = Join-Path $engineFolder "EncrypticAntiCheat\ThirdParty\encryptic\bin\Win64"
        New-Item -ItemType Directory -Path $ueDllDir -Force | Out-Null
        Copy-Item $dllSource (Join-Path $ueDllDir "encryptic_core.dll")
        $engineLabel = "Unreal Engine 5"
        $installSteps = @"
Copy engine/EncrypticAntiCheat to YourGame/Plugins/
Enable plugin in UE. Copy encryptic_config.json next to your .exe or load via FEncrypticModule.

Call FEncrypticModule::Get().Start(Config) from GameInstance.
"@
    }
    "4" {
        Copy-Item -Recurse (Join-Path $Root "engines\godot\addons\encryptic_anticheat") (Join-Path $engineFolder "addons\encryptic_anticheat") -Force
        Copy-Item -Recurse (Join-Path $Root "engines\godot\gdextension") (Join-Path $engineFolder "gdextension") -Force
        $gdDll = Join-Path $engineFolder "gdextension\bin"
        New-Item -ItemType Directory -Path $gdDll -Force | Out-Null
        Copy-Item $dllSource (Join-Path $gdDll "encryptic_gdextension.dll")
        $engineLabel = "Godot 4"
        $installSteps = @"
Copy engine/addons and engine/gdextension into your Godot project.
Set Encryptic as autoload. Ship encryptic_config.json with your game binary.
"@
    }
    "5" {
        Copy-Item (Join-Path $Root "examples\native\main.cpp") (Join-Path $engineFolder "example_main.cpp")
        $engineLabel = "Native C++"
        $installSteps = @"
Link encryptic_core.dll + encryptic_core_static.lib from build/core/Release/

  encryptic_start_from_file("encryptic_config.json");
  while (running) encryptic_tick();
"@
    }
    "6" {
        Copy-Item -Recurse (Join-Path $Root "engines\roblox\ServerScriptService\Encryptic") (Join-Path $engineFolder "ServerScriptService\Encryptic") -Force
        Copy-Item (Join-Path $Root "engines\roblox\README.md") (Join-Path $engineFolder "README.md")
        $engineLabel = "Roblox"
        $installSteps = @"
Roblox uses SERVER-SIDE anti-cheat only (no native DLL).

Copy engine/ServerScriptService/Encryptic into your experience.
In ServerScriptService: require(Encryptic).start({ maxSpeed = 32 })

Also run PC launcher with encryptic_core for external telemetry if needed.
"@
    }
    default {
        Copy-Item -Recurse (Join-Path $Root "engines\unity-il2cpp\Runtime") (Join-Path $engineFolder "Encryptic") -Force
        New-Item -ItemType Directory -Path (Join-Path $engineFolder "Plugins\x86_64") -Force | Out-Null
        Copy-Item $dllSource (Join-Path $engineFolder "Plugins\x86_64\encryptic_core.dll")
        $engineLabel = "Unity IL2CPP"
        $installSteps = @"
Copy 'engine/' contents into your Unity project:
  - Assets/Encryptic/          (scripts)
  - Assets/Plugins/x86_64/     (encryptic_core.dll)
  - Assets/StreamingAssets/encryptic_config.json

In first scene:
  Encryptic.AntiCheat.EncrypticAntiCheat.StartFromConfig();
"@
    }
}

# Telemetry starter
if ($enableTelemetry -and $hasNode) {
    $telemetryBat = @"
@echo off
cd /d "%~dp0"
echo Starting Encryptic telemetry server on port $telemetryPort ...
node "%~dp0..\..\server\examples\node\server.js"
pause
"@
    # Patch port in copied script - create local server launcher
    $localServer = @"
const http = require("http");
const PORT = $telemetryPort;
const API_KEY = "$apiKey";
function readBody(req) { return new Promise(r => { let d=""; req.on("data",c=>d+=c); req.on("end",()=>r(d)); }); }
const server = http.createServer(async (req, res) => {
  if (req.headers["x-encryptic-key"] !== API_KEY) { res.writeHead(401); return res.end("unauthorized"); }
  const body = await readBody(req);
  if (req.url === "/v1/violations" && req.method === "POST") { console.log("[violation]", body); res.writeHead(200); return res.end('{"ok":true}'); }
  if (req.url === "/v1/heartbeat" && req.method === "POST") { console.log("[heartbeat]", body); res.writeHead(200); return res.end('{"ok":true}'); }
  res.writeHead(404); res.end("not found");
});
server.listen(PORT, () => console.log("Encryptic telemetry http://127.0.0.1:" + PORT));
"@
    $localServer | Set-Content (Join-Path $distDir "start_telemetry_server.js") -Encoding UTF8
    @"
@echo off
cd /d "%~dp0"
echo Encryptic telemetry + heartbeat receiver
node start_telemetry_server.js
"@ | Set-Content (Join-Path $distDir "START_TELEMETRY.bat") -Encoding ASCII
}

# INSTALL readme
$readme = @"
ENCRYPTIC ANTI-CHEAT — INSTALL PACKAGE
======================================
Game: $gameName
Engine: $engineLabel
Preset: $preset
Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm")

WHAT'S INCLUDED (REAL PROTECTIONS)
---------------------------------
[Client] DLL injection guard, debugger (10+ checks), API hooks, memory integrity
[Client] Process + service scan, overlay detection, synthetic input, watchdog thread
[Client] Self-integrity CRC on encryptic_core.dll, launch watermark
[Client] Telemetry JSON + heartbeat (when server running)
[Server] See ../../docs/server-validation.md for authoritative gameplay checks

FILES
-----
encryptic_core.dll       - native anti-cheat library
encryptic_config.json    - all guard toggles + telemetry URLs
encryptic_watermark.json - launch splash settings
engine/                  - copy into your game project

QUICK START
-----------
1. Run START_TELEMETRY.bat (keep window open during dev)
2. $installSteps
3. Ship encryptic_config.json beside your game .exe (or StreamingAssets)
4. Call encryptic_tick() every frame / Update()

VERIFY
------
- Launch game: watermark should appear
- Open Cheat Engine: should trigger debugger violation in telemetry console
- See docs/server-validation.md for server-side rules

DOCS
----
../../docs/getting-started.md
../../docs/server-validation.md
../../docs/anti-bypass.md
"@
$readme | Set-Content (Join-Path $distDir "INSTALL.txt") -Encoding UTF8

# --- Copy to game project if path given ---
if ($gamePath) {
    Write-Host "[4/5] Copying to game project: $gamePath" -ForegroundColor Green
    if (-not (Test-Path $gamePath)) {
        Write-Host "  Path not found — skipped." -ForegroundColor Yellow
    } else {
        switch ($engineChoice) {
            "2" {
                $assets = Join-Path $gamePath "Assets"
                Copy-Item -Recurse (Join-Path $engineFolder "Encryptic") (Join-Path $assets "Encryptic") -Force
                $plugins = Join-Path $assets "Plugins\x86_64"
                New-Item -ItemType Directory -Path $plugins -Force | Out-Null
                Copy-Item $dllSource (Join-Path $plugins "encryptic_core.dll")
                $sa = Join-Path $assets "StreamingAssets"
                New-Item -ItemType Directory -Path $sa -Force | Out-Null
                Copy-Item (Join-Path $distDir "encryptic_config.json") (Join-Path $sa "encryptic_config.json")
            }
            "3" {
                $plugins = Join-Path $gamePath "Plugins"
                Copy-Item -Recurse (Join-Path $engineFolder "EncrypticAntiCheat") (Join-Path $plugins "EncrypticAntiCheat") -Force
            }
            "6" {
                Write-Host "  Roblox: manually insert ServerScriptService scripts in Studio." -ForegroundColor Yellow
            }
            default {
                if ($engineChoice -eq "1" -or $engineChoice -eq "") {
                    $assets = Join-Path $gamePath "Assets"
                    Copy-Item -Recurse (Join-Path $engineFolder "Encryptic") (Join-Path $assets "Encryptic") -Force
                    $plugins = Join-Path $assets "Plugins\x86_64"
                    New-Item -ItemType Directory -Path $plugins -Force | Out-Null
                    Copy-Item $dllSource (Join-Path $plugins "encryptic_core.dll")
                    $sa = Join-Path $assets "StreamingAssets"
                    New-Item -ItemType Directory -Path $sa -Force | Out-Null
                    Copy-Item (Join-Path $distDir "encryptic_config.json") (Join-Path $sa "encryptic_config.json")
                }
            }
        }
        Write-Host "  Copied." -ForegroundColor DarkGreen
    }
} else {
    Write-Host "[4/5] Skipped game project copy (no path)." -ForegroundColor DarkGray
}

Write-Host "[5/5] Done!" -ForegroundColor Green
Write-Host ""
Write-Host "  Package: dist\$distName" -ForegroundColor Cyan
Write-Host "  1) Run dist\$distName\START_TELEMETRY.bat" -ForegroundColor White
Write-Host "  2) Follow dist\$distName\INSTALL.txt" -ForegroundColor White
Write-Host "  3) Read docs\server-validation.md for server rules" -ForegroundColor White
Write-Host ""

exit 0
