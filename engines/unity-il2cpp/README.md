# Unity IL2CPP integration

1. Build `encryptic_core.dll` and copy to `Assets/Plugins/x86_64/`
2. Copy `Runtime/EncrypticAntiCheat.cs` into your project
3. Add to a `DontDestroyOnLoad` object

```csharp
EncrypticAntiCheat.Start(new EncrypticConfig {
    Watermark = { GameTitle = "My Game", Enabled = true },
    ProcessGuard = true,
    DllGuard = true,
});
```

The native core shows the **Encryptic launch watermark** automatically on start (EAC-style).
