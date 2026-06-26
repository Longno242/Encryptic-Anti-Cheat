#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FSentinelViolation
{
    int32 Type = 0;
    int32 Severity = 0;
    FString Message;
    FString Detail;
};

struct FSentinelConfig
{
    bool bDllGuard = true;
    bool bDebuggerGuard = true;
    bool bHookGuard = true;
    bool bMemoryGuard = true;
    bool bThreadGuard = true;
    bool bTimingGuard = true;
    TFunction<void(const FSentinelViolation&)> OnViolation;
};

class SENTINELANTICHEAT_API FSentinelModule : public IModuleInterface
{
public:
    static FSentinelModule& Get();

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void Start(const FSentinelConfig& Config);
    void Stop();
    bool Tick(float DeltaTime);

private:
    FSentinelConfig ActiveConfig;
    bool bRunning = false;
    FTSTicker::FDelegateHandle TickerHandle;
};
