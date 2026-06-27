# Native integration

```cmake
target_link_libraries(YourGame PRIVATE encryptic_core_static)
```

```cpp
auto cfg = anticheat::Config::from_preset(anticheat::ProtectionPreset::Balanced);
cfg.watermark.game_title = "My Game";
anticheat::Encryptic::instance().start(cfg);
```

Preview watermark only: run `test.bat` from repo root.
