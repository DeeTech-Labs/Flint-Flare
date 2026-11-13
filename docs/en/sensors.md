# Sensors and system settings

Flare uses the shared IgniteSDK Core for sensor data. Diagnostics opens a metadata-only catalog; it does not activate sampling. Select a source and press **Start diagnostics** to create a session-owned 30-second diagnostic lease. Flare renews the lease while this panel remains visible and the connection remains active. Reads are limited by the source interval and to at most 5 Hz. Closing the panel, minimizing/hiding the window, or disconnecting stops diagnostics; reconnect does not resume it. A lost stop command is bounded by the device lease expiry.

Catalog status is a cache snapshot. Values appear only for valid samples. Pending, initializing, absent, stale and failed states do not display placeholder values. Simulation sources are explicitly marked. The local simulator provides environmental, ambient light, IMU and chip-temperature sources; chip temperature is not ambient temperature. Values, availability, status, reset and forward time steps are controlled explicitly and pass through the same Core. A forward time step can expire a lease. The simulator does not execute a connected watch's Lua locally.

Display/audio controls use confirmed `settings_get` results. Backlight and mute support require separate device capability flags; an ALS source alone does not establish backlight support. Automatic brightness additionally requires an ALS source. Manual/fallback brightness accepts 5–100%. Changes are acknowledged and reread before success is shown. A hardware failure after policy persistence keeps the original error visible while Flare rereads the durable policy; a readback failure is reported alongside it. The simulator stores explicit virtual backlight/mute output through Core callbacks; the panel labels these outputs as simulation. It does not control physical desktop audio or brightness.

Time synchronization acknowledges Core persistence and then polls `time_status` for up to five seconds. RTC failure is reported separately from accepted Core time. No-RTC devices confirm Core time only. This is not a physical RTC or power-cut validation.

## Build and validation

Integration uses SDK ContractRevision0x20261114, commit `6e7a853` pinned in `platform/`. Normal builds use the submodule; FLINT_PLATFORM_DIR remains an explicit development override.

```powershell
cmake -S . -B build/sensors
cmake --build build/sensors --config Debug
ctest --test-dir build/sensors --output-on-failure -C Debug
```

Use the normal Qt/compiler environment for the platform. `sensor_sessions` covers simulator catalog/no default demand, diagnostic start/stop/expiry/disconnect, virtual states and values, and time confirmation/error handling. Physical sensors, USB/BLE behavior, RTC, backlight/audio output, power interruption and GUI layout need separate hardware/manual validation. No RAM, latency, power or accuracy guarantees are claimed.

## SDK revision

The sensor SDK is pinned to commit `6e7a853` in `platform/`. Normal builds use this submodule; an external FLINT_PLATFORM_DIR is only needed for deliberate SDK development.
