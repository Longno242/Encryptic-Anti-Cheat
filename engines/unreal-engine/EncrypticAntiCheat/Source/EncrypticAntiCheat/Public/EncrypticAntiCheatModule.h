#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FEncrypticViolation
{
    int32 Type = 0;
    int32 Severity = 0;
    FString Message;
    FString Detail;
};

struct FEncrypticWatermarkSettings
{
    bool bEnabled = true;
    FString GameTitle = TEXT("Your Game");
    FString BrandLine1 = TEXT("ENCRYPTIC");
    FString BrandLine2 = TEXT("ANTI-CHEAT");
};

struct FEncrypticConfig
{
    bool bDllGuard = true;
    bool bDebuggerGuard = true;
    bool bHookGuard = true;
    bool bProcessGuard = true;
    bool bOverlayGuard = true;
    FEncrypticWatermarkSettings Watermark;
    TFunction<void(const FEncrypticViolation&)> OnViolation;
};

class ENCRYPTICANTICHEAT_API FEncrypticModule : public IModuleInterface
{
public:
    static FEncrypticModule& Get();

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void Start(const FEncrypticConfig& Config);
    void Stop();
    bool Tick(float DeltaTime);

private:
    FEncrypticConfig ActiveConfig;
    bool bRunning = false;
    FTSTicker::FDelegateHandle TickerHandle;
};
