Place built `encryptic_core.dll` here after running:

```
cmake -B build -DENCRYPTIC_BUILD_SHARED=ON
cmake --build build --config Release
copy build\core\Release\encryptic_core.dll .
```
