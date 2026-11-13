# Как расширять Flare

[Оглавление](INDEX.md) · [English](../en/DEVELOPMENT.md) · [CONTRIBUTING](../../CONTRIBUTING.md)

## Первый проход

Соберите [проект](BUILD.md), запустите Simulator и нужный CTest. Не нужен физический
USB/BLE для изменения QML, model или parser test. [Архитектура](ARCHITECTURE.md) объясняет
текущие session/request IDs, cancellation и generation. UI — `qml/features/` и общие
components; backend — `src/platform_app/`; Bluetooth lifecycle — `src/device/`.

## Добавить экран/операцию

1. Найдите существующий controller: PlatformClient — соединение/RPC, IgniteHub — packages/
   grants, Studio — compiler subprocess, Preview — scene/input, CompanionService — host data,
   Sensors/BackgroundServices — bounded diagnostics/management. Не дублируйте их состояния в QML.
2. Новую бизнес-операцию реализуйте C++ controller/model. QObject parent/affinity, QPointer/
   connection generation и cancellation нужны для callbacks после смены устройства/экрана.
   Не показывайте отправленный intent как подтверждённое durable состояние.
3. Переиспользуйте `AppCard`, `AppButton`, Theme и layout patterns. Для collections используйте
   стабильную model/dataChanged, чтобы monitoring не пересоздавал нажатые controls. Long text,
   narrow/wide, RU/EN, inactive/disconnected/loading/error проверяйте отдельно.
4. User-visible source strings — английские `tr/qsTr`, translations JSON обоих языков,
   не .ts/.qm. [Локализация и placeholders](LOCALIZATION.md). Динамическое сообщение должно
   пере-переводиться при смене языка; не кешируйте готовый translated status навсегда.
5. Если wire/API уже есть, используйте SDK codecs и Method IDs. Новый общий контракт сначала
   меняется в owning SDK/spec, compiler/validators/tests; затем намеренный pin в обоих
   потребителях. QML не парсит байты и не реализует grants. [SDK extension guide](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/PLATFORM-GUIDE.md).

## Добавить диагностический или host backend

Catalog/metadata не включает sampling. Diagnostic telemetry требует явного consent,
lease, частотных пределов, coalescing/backpressure и остановки demand при закрытии/disconnect.
App grants и диагностическое разрешение — разные сущности. Background services устройства
при закрытии monitoring panel продолжают работу; Stop посылается только явным действием.
Не раскрывайте service Lua state/IPC/raw readings в metadata diagnostics.

Companion host metrics требуют локального согласия, demand/binding и app permission. Новый
backend не должен начинать sampling от одной аутентификации. Отмечайте unsupported backend,
не подставляйте данные другой ОС. Simulator предоставляет virtual backend тем же Core,
runtime и authorizer; remote Preview показывает execution устройства, без второй app VM.

## Проверки по подсистеме

| Правка | Проверки |
| --- | --- |
| QML/layout/localization | desktop_offline, captures wide/narrow, оба языка и accessibility labels |
| RPC/session/time | companion_sessions; malformed/truncated/timeout/disconnect/old generation |
| Preview/input | preview_application_api; focus/cancel, switching/stale release |
| Studio/resources | studio_icon_resources и SDK compiler/resource tests; PATH Python/Pillow |
| Sensors | sensor_sessions и SDK sensor_core/broker/tooling, explicit close/disconnect |
| Services | background_service_sessions/background_service_ui и SDK background_core/service_isolation |
| Общий SDK контракт | SDK Lua/no-Lua, OS host/tooling/ESP32 build, pin и миграция packages |

`ctest -N` покажет реальные tests текущей конфигурации. Флаги `BUILD_TESTING` и
`FLINT_ENABLE_LUA` управляют частью tests. [Полные команды и capture](BUILD.md).
Smoke использует изолированный simulator и не подключается к физическому устройству.
Не называйте его проверкой firmware updates, hardware BLE bonding, power loss или батареи.

Документируйте пользовательское действие в USER-GUIDE, механизм — в ARCHITECTURE,
точный SDK wire — у владельца SDK. Меняйте Unreleased changelog, RU/EN и ссылки вместе.
