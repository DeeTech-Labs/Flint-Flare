[Русский оригинал](../ru/ARCHITECTURE.md)

# Flare architecture

The QML shell and reusable visual components remain under `qml/`. Existing icons, fonts, shaders, localization and tray integration are independent of the device protocol.

`src/platform_app/` contains PlatformClient, IgniteHub, Studio, Preview and CompanionService. These services use binary typed method IDs and the shared v1 codecs. They do not expose old textual commands or a compatibility facade to QML.

`src/device/` contains only the reusable Qt Bluetooth/BlueZ hardware lifecycle. `platform/` is the pinned Flint IgniteSDK submodule containing the portable Core, package manager, Runtime, ResourceManager, event queue, renderer, compiler and contracts. The simulator executes that same Core in an isolated temporary directory.

The complete [shared specification](../../platform/README.md) defines installed formats, consent, transactions, trust and explicit unsupported features.


Demand-driven Companion uses the [shared SDK contract](../../platform/docs/en/COMPANION.md). Ignite Hub does not activate sampling. Local consent, active demand, binding and a supported source are all required. There is no permissions polling. Events do not complete pending requests or reset timeouts. CPU/RAM collection currently supports Linux only. `FLINT_PLATFORM_DIR` is an explicit development override; normal builds use pinned `platform/`.

PlatformClient keeps a separate bounded slot and timeout for resumable `install_commit`; ordinary commands and Companion samples continue while the SDK validates the package. The queue holds at most 64 requests, samples coalesce, and management commands precede queued samples. BLE serializes frames and signals readiness for the next frame; callbacks from an old connection generation are ignored.

With `CapSessionClose`, USB shutdown sends `session_close` and asynchronously waits up to 100 ms for its ACK. Late installation replies cannot complete shutdown. Without confirmation, check the installation catalog after reconnecting. The SDK provider lease releases an abandoned binding.
