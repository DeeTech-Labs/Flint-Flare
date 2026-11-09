[Русский оригинал](../ru/BUILD.md)

# Build

See [README](../../README.md) for Qt dependencies and the normal CMake commands. CTest builds and checks the portable runtime as well as the actual QML application using a disposable offline simulator.

The `platform/` directory is a Git submodule of
[Flint IgniteSDK](https://github.com/DeeTech-Labs/Flint-IgniteSDK), pinned to a commit.
Clone with `--recurse-submodules`, or run `git submodule update --init --recursive`
before configuring. Shared changes are made in the SDK repository and consumed by
updating the submodule pointer; no source export or copy step exists.

Compile a local Ignite with `python3 platform/tools/compile_ignite.py project.json output.ignit`, or use Ignite Studio. Source font/image assets require Pillow. The compiler validates resource paths against the selected project directory.

Use four-space indentation. Project formatting lives in `.editorconfig` and `.clang-format`. Icons, generic QML components, fonts and shaders are retained as the visual resource library.

## Application API 1

Both consumers pin SDK `c686975f71be708a9600a7d2f160a9dfe900679e` (local SDK branch `application-api-1`). Package/Companion formats are unchanged; new Lua projects use app_api: 1 and the compiler requires capability 128. Rebuild Flare with this submodule. The SDK commit currently exists locally; publish it to its origin before distributing parent commits.

The local simulator schedules Core through nextWake() and updates Preview on scene changes. The simulator image accepts mouse press/release/hold, Space/Enter, Up/Down and wheel input. Core::postInput enforces input.focused and Runtime generation; stale releases cannot reach another application run. Hiding Preview removes input focus without stopping the simulated active app. Physical watch input injection over Companion is not added.

Private state belongs to the simulator Core for its session; starting a new simulator creates fresh temporary storage. No battery telemetry is synthesized. Remote Preview paints watch-provided scenes using validated package resources without replaying Lua on the computer. Build platform/examples/counter/project.json with Studio and install it into the simulator to try persistence.

CTest preview_application_api exercises the actual Qt timer, Preview input, delayed private saves, relaunch, stale generations and disconnect. It is enabled with BUILD_TESTING and FLINT_ENABLE_LUA and uses no physical device.
