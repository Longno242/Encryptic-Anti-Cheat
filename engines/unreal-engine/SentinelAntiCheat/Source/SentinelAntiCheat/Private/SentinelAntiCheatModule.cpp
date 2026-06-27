#include "SentinelAntiCheatModule.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

extern "C" {
    void sentinel_start_default();
    void sentinel_tick();
    void sentinel_stop();
}

FSentinelModule& FSentinelModule::Get()
{
    return FModuleManager::LoadModuleChecked<FSentinelModule>("SentinelAntiCheat");
}

void FSentinelModule::StartupModule()
{
#if PLATFORM_WINDOWS
    const FString DllPath = FPaths::Combine(
        FPaths::ProjectPluginsDir(),
        TEXT("SentinelAntiCheat/ThirdParty/sentinel/bin/Win64/sentinel_core.dll"));

    if (FPaths::FileExists(DllPath))
    {
        FPlatformProcess::GetDllHandle(*DllPath);
    }
#endif
}

void FSentinelModule::ShutdownModule()
{
    Stop();
}

void FSentinelModule::Start(const FSentinelConfig& Config)
{
    ActiveConfig = Config;
    if (bRunning)
    {
        return;
    }

    sentinel_start_default();
    bRunning = true;

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FSentinelModule::Tick));
}

void FSentinelModule::Stop()
{
    if (!bRunning)
    {
        return;
    }

    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    sentinel_stop();
    bRunning = false;
}

bool FSentinelModule::Tick(float /*DeltaTime*/)
{
    if (bRunning)
    {
        sentinel_tick();
    }
    return true;
}

IMPLEMENT_MODULE(FSentinelModule, SentinelAntiCheat)
