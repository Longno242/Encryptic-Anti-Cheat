# Unity Mono integration

Copy `Runtime/EncrypticMonoAntiCheat.cs` into your project.

```csharp
EncrypticMonoAntiCheat.Start(new EncrypticMonoConfig {
    UseNativeCore = true,
    Watermark = { GameTitle = "My Game" },
});
```

Ship `encryptic_core.dll` under `Plugins/x86_64/` for native watermark + advanced guards.
