[Русский оригинал](../../CONTRIBUTING.md)

# Contributing to Flint Flare

Thanks for helping improve **Flint Flare**, the companion for **FlinT** devices
running **FlinT OS**, by **DeeTech Labs**.

## Before you start

- Read [README](../../README.md), [architecture](ARCHITECTURE.md) and the
  [Companion protocol](COMPANION-PROTOCOL.md).
- Check [CHANGELOG.md](../../CHANGELOG.md) for development notes.
- Questions and ideas → [Discussions](https://github.com/DeeTech-Labs/Flint-Flare/discussions).
- Bugs and concrete features → [Issues](https://github.com/DeeTech-Labs/Flint-Flare/issues).
- Support routing → [SUPPORT.md](SUPPORT.md).

## Development setup

Install Qt 6.5+ (Quick, Quick Controls, Quick Dialogs, Serial Port), CMake 3.21+,
a C++20 compiler and Python 3/Pillow for compiler and integration tests. See [BUILD.md](BUILD.md).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

Tests use the shared Core and a disposable offline simulator; they do not need physical hardware.
The GitHub workflow builds with Qt 6.10.2 on Linux, macOS and Windows for PRs and pushes to `main`.
Run hardware checks separately when changing USB or Bluetooth behavior.

## Code organization

- QML owns presentation; retain its design system, icons and visual resources.
- `src/platform_app/` owns application services; `src/device/` owns the reusable Bluetooth lifecycle.
- `platform/` is the pinned IgniteSDK Git submodule.
- Keep serial I/O asynchronous and the UI thread responsive.
- Match complete protocol replies, handle fragmented input and preserve input bounds.
- Use capabilities from INFO; do not gate features on a hard-coded model name.
- Use the shared binary v1 handshake and development contract revision; do not add textual compatibility shims.
- Use four-space indentation without tabs.
- Document protocol changes in both repositories and test failure behavior.
- Keep generated builds, device captures and local Qt Creator settings out of commits.

## Branching and PRs

- Do not push directly to `main`.
- Branch from current `main`: `feature/…`, `fix/…`, `chore/…`, `docs/…`.
- Open a small, reviewable PR. Explain behavior, compatibility and verification.
- Changes to firmware belong in [FlinT OS](https://github.com/DeeTech-Labs/FlinT-OS).

## Commit messages

Use English imperative [Conventional Commits](https://www.conventionalcommits.org/):

```text
feat(usb): Add device capability discovery
fix(session): Reject incomplete status snapshots
docs: Clarify Qt Creator setup
```

## Naming and license

**Flint Flare** (application), **FlinT OS** (firmware), **FlinT Spark** (first
device), **FlinT** (product line), **DeeTech Labs** (organization).

See [licensing scope](LICENSING.md), [LICENSE](../../LICENSE) and [NOTICE](../../NOTICE).
Participation follows the [Code of Conduct](CODE_OF_CONDUCT.md).

## Documentation and contribution terms

Russian is canonical; update Russian documentation and its English translation together. Authors retain copyright. Contributions use the applicable component license; they do not imply assignment or unrestricted commercial relicensing. See [licensing scope](LICENSING.md).

## A small change

Start with [build](BUILD.md), Simulator and [controller map](DEVELOPMENT.md), not all formats.
Pillow must exist before configure; Linux needs QtDBus. Use one Qt/compiler kit, --config/-C
Debug for Visual Studio. Artifacts only under owning build/temp; independent SDK platform/build.
Baseline: diff/build/CTest/RU/EN/links. QML: desktop_offline+captures; sessions: companion_sessions;
Preview: preview_application_api; Studio: studio_icon_resources; sensors: sensor_sessions;
services: background_service_sessions/background_service_ui. ctest -N lists actual configured
names. Shared contracts require owning SDK tests and OS consumer build, never duplication.
New data sources cannot sample merely on authentication. Report OS/kit/transport/revisions,
safe minimal reproduction, expected/actual; no private data/keys. PR includes behavior,
migration/security, executed commands, UI captures, Unreleased notes and unverified hardware.
Simulator tests do not prove physical BLE/USB/power-loss/battery behavior.
