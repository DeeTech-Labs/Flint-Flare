# Сборка и проверка Flare

[Оглавление](INDEX.md) · [English](../en/BUILD.md) · [Пользование](USER-GUIDE.md) · [Разработка](DEVELOPMENT.md)

## Инструменты

Git, CMake3.21+, C/C++20, Qt6.5+ с Quick/Quick Controls/Quick Dialogs, SerialPort,
Bluetooth, Widgets, ShaderTools; на Linux также QtDBus. Установите соответствующие
development modules в своём Qt kit. Не смешивайте Qt MinGW с MSVC или разные architectures.
CI настроен на Qt6.10.2/Linux/macOS/Windows; текущие локальные проверки выполнены на
Windows Qt6.12 MinGW15.1. Это не подтверждение физического транспорта на всех ОС.

Python3.11+ и Pillow нужны **до CMake configure**: общий SDK генерирует factory assets
даже без tests. CairoSVG/system Cairo нужны только для SVG conversion. Lua включена в SDK.
Активируйте окружение Qt/compiler и Python перед запуском Studio: он ищет Python в PATH.

## Сборка из текущего checkout

Из корня Flare, после `git status`/`git submodule status`:

```sh
git submodule update --init --recursive
python -m pip install Pillow
cmake -S . -B build/docs-flare -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/Qt/kit
cmake --build build/docs-flare --config Debug --parallel 2
ctest --test-dir build/docs-flare -C Debug --output-on-failure
```

Замените `/path/to/Qt/kit` реальным kit, либо опустите prefix при system Qt. В Windows
Developer PowerShell/MSVC executable — `build/docs-flare/Debug/flint_flare.exe`. Для Windows Ninja/MinGW — `build/docs-flare/flint_flare.exe`, Linux — `build/docs-flare/flint_flare`, macOS single-config — `build/docs-flare/flint_flare.app/Contents/MacOS/flint_flare`. Для Ninja добавьте `-G Ninja`
при первом configure. Не меняйте generator/compiler в существующем build.
Успех — запуск окна и `100% tests passed`, а не только скомпилированный executable.

SDK pin6e7a853 / contract0x20261114 / FLM3; обычный путь — platform/. Если cache содержал
sibling override, задайте `-DFLINT_PLATFORM_DIR=<Flare checkout>/platform`. Закреплённый SHA опубликован в main SDK; не заменяйте pin на latest.
Generated registries/factory/examples находятся в build/.../platform/generated, source
snapshots не используются вместо генерации. Старые root manifest fields не поддерживаются.

## Offline и UI-проверки

CTest desktop_offline запускает настоящее QML приложение с временным Simulator, без
подключения USB/BLE. Для ручного smoke/capture задайте `QT_QPA_PLATFORM=offscreen` и
`QT_QUICK_BACKEND=software` (в PowerShell `$env:ИМЯ='значение'`), затем:

```sh
build/docs-flare/flint_flare --smoke --capture build/docs-flare/flare.png
```

Для Visual Studio добавьте Debug/ и .exe в путь; для macOS используйте bundle executable, указанный выше. Создаются дополнительные `.watch.png`,
`.watch-narrow.png`, `.overview.png`, `.overview-narrow.png` и другие состояния рядом с PNG.
background_service_ui устанавливает реальный thermometer-service в simulator и сохраняет
wide RU/narrow EN capture. Это не mock layout с нарисованными строками и не physical test.
`ctest -N` перечисляет targets; `-R background_service` выбирает service controller/UI.
SDK tests включены при BUILD_TESTING; FLINT_ENABLE_LUA влияет на Lua tests.

## Compiler и Preview

Studio вызывает общий compile_ignite.py из SDK resources отдельным Python subprocess.
Для CLI: `python platform/tools/compile_ignite.py platform/examples/counter/project.json
build/docs-flare/counter.ignit`. Проверяйте пакет также ignite-check из SDK build.
Simulator исполняет тот же Core по nextWake, хранит private data в session temp directory,
не выдумывает физическую батарею и помечает sensor samples simulation. Закрытие/новый
simulator session не является проверкой durable reboot физической Flash.
Remote Preview показывает scene устройства, без второй копии его Lua. Локальный Preview
передаёт мышь/клавиши/колесо через focus/input generation; remote input не добавлен.
[API/examples](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/APP-QUICKSTART.md).

Build/test/temp artifacts только в owning build/ и temp/. Для отдельно собираемого SDK
submodule используйте platform/build/. [Матрица проверок](DEVELOPMENT.md).
