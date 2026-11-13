# Extending Flare

[Index](INDEX.md) · [Canonical Russian](../ru/DEVELOPMENT.md) · [Contributing](CONTRIBUTING.md)

Build and run Simulator/relevant tests first; QML/model/parser work needs no physical device.
ARCHITECTURE explains requests/sessions/generations. qml/features and shared components own
presentation, src/platform_app controllers, src/device Bluetooth lifecycle.

Use existing PlatformClient (connection/RPC), IgniteHub (packages/grants), Studio (compiler),
Preview (scene/input), CompanionService (host data), Sensors/BackgroundServices. Do not
duplicate controller state in QML. New business operations belong in C++ model/controller;
respect QObject parent/affinity, QPointer, connection generation/cancellation. Intents are
not confirmed durable state. Reuse AppCard/AppButton/Theme and stable models/dataChanged;
test long/narrow/wide/RU/EN/inactive/disconnected/loading/error states. Source strings use
English tr/qsTr with both JSON catalogs, not .ts/.qm. Retranslate cached dynamic notices
on language changes. [Localization](LOCALIZATION.md).

Use SDK codecs/Method IDs. New shared contracts start in owning SDK/spec/compiler/tests,
then deliberately update both consumer pins. QML never parses wire bytes or grants rights.
[SDK guide](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/en/PLATFORM-GUIDE.md).

Catalog metadata never implies sampling. Diagnostic telemetry requires explicit consent,
bounded lease/rate/coalescing/backpressure and cancellation on close/disconnect. Session
diagnostics and app grants differ. Closing service monitoring must not Stop device services;
Stop is explicit. Metadata must not expose Lua state/IPC/raw samples. Companion sampling
requires local consent, demand/binding and app permission, not just authentication.
Unsupported OS backends remain unsupported. Simulator shares Core/runtime/authorizer;
remote Preview renders device execution without a second application VM.

## Validation map

QML/localization: desktop_offline, wide/narrow captures, both languages/accessibility.
RPC/time/session: companion_sessions with malformed/truncated/timeout/disconnect/old-generation.
Preview: preview_application_api with focus/cancellation/switching. Studio: studio_icon_resources
and compiler/resource tests with Python/Pillow PATH. Sensors: sensor_sessions and SDK
sensor_core/broker/tooling. Services: background_service_sessions/background_service_ui and
SDK background_core/service_isolation. Shared contracts require SDK Lua/no-Lua and OS host/
tooling/firmware plus coordinated pins/migration. `ctest -N` lists configured tests;
BUILD_TESTING/FLINT_ENABLE_LUA gate some targets. [Commands/capture](BUILD.md).
Smoke is isolated simulator behavior, not firmware updating, physical bonding, battery or
power-loss validation. Put user actions in USER-GUIDE, mechanisms in ARCHITECTURE, wire truth
in SDK; update Unreleased/RU/EN/links together.
