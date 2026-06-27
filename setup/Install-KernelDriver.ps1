#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Build (optional), install, and start the EncrypticGuard kernel driver.

.NOTES
  - Requires Windows Driver Kit (WDK) to build the .sys file.
  - Development machines need test signing: bcdedit /set testsigning on
  - Reboot after enabling test signing.
#>

param(
    [switch]$EnableTestSigning,
    [switch]$SkipBuild,
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$DriverDir = Join-Path $Root "driver"
$SysPath = Join-Path $DriverDir "bin\x64\$Configuration\EncrypticGuard.sys"
$DestPath = Join-Path $env:SystemRoot "System32\drivers\EncrypticGuard.sys"
$ServiceName = "EncrypticGuard"

function Test-WdkInstalled {
    $km = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName "km" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
    return [bool]$km
}

function Enable-TestSigningMode {
    Write-Host "Enabling test signing (reboot required)..." -ForegroundColor Yellow
    bcdedit /set testsigning on | Out-Null
    Write-Host "Reboot Windows, then re-run this script." -ForegroundColor Green
}

function Build-Driver {
    if (-not (Test-WdkInstalled)) {
        throw "WDK not installed. Install 'Windows Driver Kit' from Visual Studio Installer -> Individual Components."
    }

    $msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
        -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1

    if (-not $msbuild) {
        throw "MSBuild not found."
    }

    Write-Host "Building EncrypticGuard.sys ($Configuration|x64)..." -ForegroundColor Cyan
    & $msbuild (Join-Path $DriverDir "EncrypticGuard.vcxproj") /p:Configuration=$Configuration /p:Platform=x64 /t:Rebuild
    if ($LASTEXITCODE -ne 0) {
        throw "Driver build failed."
    }
}

function Install-DriverService {
    if (-not (Test-Path $SysPath)) {
        throw "Driver binary not found: $SysPath"
    }

    Copy-Item $SysPath $DestPath -Force
    Write-Host "Copied driver to $DestPath" -ForegroundColor Green

    $existing = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
    if ($existing) {
        if ($existing.Status -eq 'Running') {
            sc.exe stop $ServiceName | Out-Null
            Start-Sleep -Seconds 1
        }
        sc.exe delete $ServiceName | Out-Null
        Start-Sleep -Seconds 1
    }

    sc.exe create $ServiceName type= kernel start= demand binPath= "$DestPath" DisplayName= "Encryptic Kernel Guard" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "sc.exe create failed."
    }

    sc.exe start $ServiceName | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "sc.exe start failed. Is test signing enabled? Run: bcdedit /set testsigning on"
    }

    Write-Host "EncrypticGuard kernel driver started." -ForegroundColor Green
    Write-Host "User-mode anti-cheat connects via \\.\EncrypticGuard" -ForegroundColor Cyan
}

if ($EnableTestSigning) {
    Enable-TestSigningMode
    exit 0
}

if (-not $SkipBuild) {
    Build-Driver
}

Install-DriverService
