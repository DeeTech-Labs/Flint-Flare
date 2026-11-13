# Flare: connecting and managing applications

[Index](INDEX.md) · [Canonical Russian](../ru/USER-GUIDE.md) · [Build](BUILD.md)

Use a compatible preflashed FlinT device, data USB and matching development contracts.
Without hardware choose Simulator on Connection; it does not verify hardware/radio/power.
Flare has no firmware updater. Building source in Studio needs Python/Pillow on Flare's
process PATH; installing an already compiled .ignit does not.

## Connect

Select USB port on Connection, click USB, wait for compatible handshake/device details.
Close a serial monitor occupying the port. First BLE pairing: retain trusted USB, click
Open pairing for 60 seconds → Scan Bluetooth → select device → BLE; compare code and confirm
through the shown trusted USB path. Cancel mismatches/no trusted path. A board may lack a
physical confirmation button. Select transport explicitly; OS 'paired' is not Core connection.
Disabled device Bluetooth needs USB to enable again. Re-pairing cannot fix a contract mismatch.

## Packages and permissions

Ignite Hub installed tab → Install Ignite → .ignit. Compile JSON first in Studio or
[SDK CLI](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/en/APP-QUICKSTART.md).
Wait for confirmed completion: transfer100% is not verification/publication. After disconnect,
reread catalog/version; lost ACK may follow durable installation. Select/launch component;
clock/theme/locale are role selections. Install/UI launch do not start services automatically.
Remove requires confirmation, factory identities are protected. Uninstall stops package instances
and removes app kv; do not assume secure erase of shared state/all stored values. Updates stop
old instances/handles and never auto-grant new permissions. Incompatible retained objects may
be hidden from executable catalog until rebuilt/reinstalled.

Card → Permissions (or permissions tab), select package, review permission/scope, Allow/Revoke,
confirm and wait for Core. Storage error can mean access denied in RAM while durable revoke
failed; an old grant may return after reboot. Temperature does not permit humidity/pressure;
background execution does not grant sensors/Companion/audio/network. Each component declares
its own operations despite package grants. Session grants do not survive reconnect/reboot;
Allow/Revoke supports persistent/session/selected-resource with the shown declared scope;
there is no arbitrary file/resource picker. Restricted/explicit-action have no generic button,
and actual backend support is still required. Registry names do not prove working API. Already delivered/stored
values cannot be recalled. [SDK access rules](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/en/LUA-API.md).

## Services

Diagnostics → Background services shows actual Core component/instance/state/reason.
Explicitly allow app.background.execute; Start never grants it. Grant sensors separately in
Ignite Hub, then Start. Denials can be permission/model/energy/memory/quota faults.
Pause cancels demand/timers, Resume continues; Stop frees the instance; Revoke execution
stops and prevents restart. For boot startup separately grant app.background.autostart and
enable Start this service automatically. Execution grant, autostart grant and preference are
all required, off by default.

Closing application UI, Diagnostics, Preview or Flare is not Stop on the physical device.
Disconnect ends monitoring, not persistent execution. Manual pause is not automatically resumed;
energy pause follows Core policy. Faulted instances need explicit restart after fixing cause.
Failed crash-latch persistence can leave old boot preference after reboot. Host tests do not
guarantee two physical VMs. [Detailed policy](background-services.md).

Simulator disconnect destroys its virtual device/session storage; a new Simulator is a new session. This differs from unplugging physical watches.

## Sensors/settings

Diagnostics → Sensors catalog is metadata only. Explicitly start bounded diagnostic sampling;
Stop, closing page/window and disconnect end its lease/demand. This session consent is separate
from app grants; raw IMU ODR does not stream over BLE by default. My watch → Watch settings
provides time sync, UTC offset minutes, Bluetooth and available outputs. ALS/brightness/mute
require configured hardware; not_supported cannot be fixed with a grant. RTC may report partial
Core success/RTC failure. Unknown battery is not 0% or charging.
PC CPU/RAM requires app companion.pc.metrics grant with exact cpu,ram scope, local Flare policy
and an active consumer. Its backend is currently Linux-only; no fake values on Windows/macOS.

## Troubleshooting/recovery

No USB: cable → OS port → close monitor → port/USB selection → handshake/version.
No BLE: USB → Bluetooth enabled → pairing window → scan/select/BLE. Incompatible: align
firmware/factory/Flare/package contract; never erase to bypass. not_supported: unimplemented or
unconfigured; unavailable: configured missing/failed/stale source, inspect reason/age.
Permission denied: declaration → scope → confirmed grant → package generation. Pending: wait
for initialization/deadline; nil/zero is not an absent measurement. Interrupted install: reconnect,
reread catalog/version, then retry/resume. Service denial: grant → model/energy/memory/fault.
Studio PIL/Python failures: launch Flare from the right environment; subprocess uses PATH.
[Support](../../SUPPORT.md) needs version/OS/model/transport/error and safe reproduction, not keys.

When connected My watch → Watch settings → Restore factory components → confirm clears role
selections/activates factory components. It is not format/reflash/stop-all-services. Stop services
separately. If Core/factory/storage cannot start this action may be unavailable:
[OS recovery](https://github.com/DeeTech-Labs/FlinT-OS/blob/main/docs/en/getting-started/device-use.md).
Firmware uses OS/ESP-IDF plus matching factory.bin; partition changes need a separate data
migration plan. Sensor/service integration changed no partition layout.
