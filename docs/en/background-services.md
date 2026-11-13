# Background services in Flare

The Diagnostics page lists installed service components and their confirmed Core state, reason, instance generation, memory usage and bounded execution counters. Polling runs every 500 ms while the page/window is active. The protocol carries metadata, not application state, IPC messages or sensor readings.

Callback counters represent dispatch batches: one runtime tick can deliver several Lua events/timers under one shared budget. Maximum callback time covers that whole batch. The dispatch counter does not measure physical OS wakeups.

Start, stop, pause, resume and execution revocation are explicit user actions. Starting does not grant `app.background.execute`. The permission buttons request consent through the standard package permission API. Automatic start has two separate requirements: the `app.background.autostart` grant and the service's enabled automatic-start policy. Removing execution permission stops the service; package declarations alone never authorize it. Sensor and other application permissions remain in Ignite Hub.

Closing the panel or disconnecting Flare stops monitoring and sends no service-stop command. Core continues to own device services. Energy-paused services can resume when Core leaves its low-energy policy; manually paused services require an explicit resume. Actual device limits and failures are authoritative.

## Simulator

The local simulator uses the same SDK Core. Its explicit virtual policy allows two service slots, a 128 KiB background reservation limit and a 512 KiB virtual admission-availability budget. These are simulator configuration values, not measurements or hardware RAM guarantees. Dispatch wall-time accounting uses the real monotonic clock. The **Simulate low energy** switch changes only simulator policy; it does not represent a physical battery.

The SDK example `examples/thermometer-service/project.json` contains independent foreground `main` and background `worker` components, separate Lua sections and shared source includes. Install the generated package, grant execution and temperature/humidity permissions explicitly, and open its UI to see shared temperature/humidity state. Service metadata never exports those values. Automatic start is not enabled by the example declaration. Simulator disconnect destroys the local simulated device instance; it is not a physical unplug persistence test.

## Build and checks

This integration requires SDK development contract `0x20261115` and rebuilt FLM3 packages with explicit component-local budget, capabilities, permissions and logic. Root-level legacy fields are rejected. Flare pins the validated SDK commit `56292d7f49315522c5789b679543ef231937620f` in `platform/`. Initialize the checked-in submodule and use the normal default:

```powershell
git submodule update --init --recursive
cmake -S . -B build/background-services
cmake --build build/background-services --config Debug
ctest --test-dir build/background-services --output-on-failure -C Debug
```

Use the normal Qt/compiler environment; relocated SDK tests also need the intended generator/compiler in the environment. `background_service_sessions` exercises actual package installation, explicit grants, foreground/service independence, hidden monitoring, low-energy/manual pause, automatic-start flags and execution revocation through Core. Physical resource admission, battery policy, USB/BLE unplug behavior, reboot/power interruption and GUI layout need separate device/manual verification. No latency, power or RAM savings are claimed.

An existing CMake cache that points at a sibling SDK must be reconfigured with `-DFLINT_PLATFORM_DIR=<Flare checkout>/platform` before validating the pinned consumer.
