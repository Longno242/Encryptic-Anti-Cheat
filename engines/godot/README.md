# Godot 4 integration

Copy `addons/encryptic_anticheat/` into your project and set as autoload.

Native GDExtension: build `encryptic_core.dll` as `bin/encryptic_gdextension.dll` next to `encryptic.gdextension`.

```gdscript
$Encryptic.start({ "dll_guard": true })
```
