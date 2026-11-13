# Building and validating Flare

[Index](INDEX.md) · [Canonical Russian](../ru/BUILD.md) · [Use](USER-GUIDE.md) · [Development](DEVELOPMENT.md)

Git, CMake3.21+, C/C++20, Qt6.5+ Quick/Controls/Dialogs, SerialPort, Bluetooth, Widgets,
ShaderTools and Linux QtDBus development modules are required. Use one matching Qt/compiler/
architecture kit, never Qt MinGW with MSVC. CI config uses Qt6.10.2 on Linux/macOS/Windows;
local validation is Windows Qt6.12 MinGW15.1, not proof of every OS/physical transport.
Python3.11+/Pillow must be installed before configure: SDK always generates factory assets.
SVG conversion additionally needs CairoSVG/system Cairo. Lua is bundled. Launch Studio from
the intended Python environment because its compiler subprocess selects Python on PATH.

From Flare root after inspecting git/submodule status:

```sh
git submodule update --init --recursive
python -m pip install Pillow
cmake -S . -B build/docs-flare -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/Qt/kit
cmake --build build/docs-flare --config Debug --parallel 2
ctest --test-dir build/docs-flare -C Debug --output-on-failure
```

Replace the kit path or omit it for system Qt. Windows Developer PowerShell/MSVC executable:
build/docs-flare/Debug/flint_flare.exe; Windows Ninja/MinGW: build/docs-flare/flint_flare.exe; Linux: build/docs-flare/flint_flare; macOS single-config: build/docs-flare/flint_flare.app/Contents/MacOS/flint_flare.
Add -G Ninja only on first configure; never mix generators in one tree. Success includes the
window and 100% tests passed, not compilation alone. Pin845590f/contract0x20261114/FLM3 uses
platform/. Clear stale overrides with FLINT_PLATFORM_DIR=<Flare checkout>/platform. A local
pin may be unpublished: obtain matching SDK checkout/bundle, never replace with latest.
With a supplied local SDK: `git -c submodule.platform.url=<SDK path> submodule update --init platform`.
Generated registries/factory/examples live under build/.../platform/generated, not source snapshots.

## Offline/UI and apps

desktop_offline runs real QML with disposable Simulator, no USB/BLE connection. For capture
set QT_QPA_PLATFORM=offscreen and QT_QUICK_BACKEND=software (PowerShell uses `$env:NAME='value'`):

```sh
build/docs-flare/flint_flare --smoke --capture build/docs-flare/flare.png
```

Use Debug/ and .exe for Visual Studio, the bundle executable above for macOS. Additional watch/overview wide/narrow/state PNGs are
written beside it. background_service_ui installs a real thermometer-service in Simulator,
captures wide RU/narrow EN; not fake model rows or physical validation. ctest -N lists tests,
-R background_service selects controller/UI. BUILD_TESTING enables SDK tests, Lua flag gates
Lua-specific tests. [Validation map](DEVELOPMENT.md).

Studio runs SDK compile_ignite.py as a Python subprocess. CLI: `python platform/tools/compile_ignite.py
platform/examples/counter/project.json build/docs-flare/counter.ignit`; also validate with SDK
ignite-check. Simulator shares Core/nextWake, session temporary storage and explicitly virtual
sensor data, no physical battery. A new simulator session is not physical durable reboot.
Remote Preview displays device scenes without a second Lua copy; local input uses focused
generation-bound mouse/keyboard/wheel, no remote device input. [App guide](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/en/APP-QUICKSTART.md).
Artifacts belong to owning build/temp; independent SDK builds use platform/build/.
