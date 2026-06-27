#include "EncrypticAntiCheatModule.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

extern "C" {
    void encryptic_start_default();
    void encryptic_tick();
    void encryptic_stop();
}

FEncrypticModule& FEncrypticModule::Get()
{
    return FModuleManager::LoadModuleChecked<FEncrypticModule>("EncrypticAntiCheat");
}

void FEncrypticModule::StartupModule()
{
#if PLATFORM_WINDOWS
    const FString DllPath = FPaths::Combine(
        FPaths::ProjectPluginsDir(),
        TEXT("EncrypticAntiCheat/ThirdParty/encryptic/bin/Win64/encryptic_core.dll"));

    if (FPaths::FileExists(DllPath))
    {
        FPlatformProcess::GetDllHandle(*DllPath);
    }
#endif
}

void FEncrypticModule::ShutdownModule()
{
    Stop();
}

void FEncrypticModule::Start(const FEncrypticConfig& Config)
{
    ActiveConfig = Config;
    if (bRunning)
    {
        return;
    }

    encryptic_start_default();
    bRunning = true;

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FEncrypticModule::Tick));
}

void FEncrypticModule::Stop()
{
    if (!bRunning)
    {
        return;
    }

    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    encryptic_stop();
    bRunning = false;
}

bool FEncrypticModule::Tick(float /*DeltaTime*/)
{
    if (bRunning)
    {
        encryptic_tick();
    }
    return true;
}

IMPLEMENT_MODULE(FEncrypticModule, EncrypticAntiCheat)
