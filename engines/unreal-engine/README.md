# Unreal Engine integration

Copy `EncrypticAntiCheat/` into your project's `Plugins/` folder.

Place `encryptic_core.dll` at:
`Plugins/EncrypticAntiCheat/ThirdParty/encryptic/bin/Win64/`

```cpp
FEncrypticConfig Config;
Config.Watermark.GameTitle = TEXT("My Game");
FEncrypticModule::Get().Start(Config);
```
