# Flare: от подключения до управления приложениями

[Оглавление](INDEX.md) · [English](../en/USER-GUIDE.md) · [Сборка Flare](BUILD.md)

## Что понадобится

Совместимое устройство с FlinT OS, data-USB и Flare той же development contract revision.
Если готового приложения у вас нет, соберите его по BUILD. Flare не прошивает произвольные
ESP32 и не содержит firmware updater. Для знакомства без платы выберите Simulator на
странице подключения: он не доказывает работоспособность радио, питания или датчиков платы.
Для исходного проекта в Studio нужны Python и Pillow в PATH-процессе, запускающем Flare;
для установки уже собранного `.ignit` они не нужны.

## Первое подключение

1. Подключите data-USB. Откройте страницу подключения, выберите USB-порт и нажмите `USB`.
   Закройте serial monitor/terminal, если порт занят. Дождитесь сведений устройства и
   совместимого handshake. Наличие USB-порта ещё не означает успешное подключение Core.
2. Для первой BLE-привязки **сохраните доверенный USB-канал**. Нажмите `Open pairing for
   60 seconds`, затем `Scan Bluetooth`, выберите устройство и `BLE`.
3. Сравните код сопряжения и подтвердите его через показанный USB-путь подтверждения.
   Если код не совпадает или доверенного USB нет, отмените, а не подтверждайте вслепую.
   Устройство/ревизия могут не предоставлять локальную кнопку подтверждения.
4. После привязки выберите нужный транспорт явно. Автоподключение не подразумевается.
   Если Bluetooth выключен политикой часов, снова включить его можно через USB.

Успех — актуальные сведения в «Мои часы», не только статус ОС «paired». При ошибке
совместимости согласуйте firmware/factory/Flare/packages; повторная привязка не меняет ABI.

## Установить, запустить и удалить Ignite

1. В Ignite Hub / вкладке установленных приложений нажмите `Install Ignite` и выберите
   `.ignit`. JSON проекта сначала компилируется в Studio либо [SDK CLI](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/APP-QUICKSTART.md).
2. Дождитесь подтверждённого завершения. Передача 100% не равна успешной проверке и
   публикации. При disconnect перечитайте каталог после reconnect: ACK мог потеряться
   уже после фиксации установки. Не удаляйте пакет только из-за потерянного ответа.
3. В карточке выберите исполняемый компонент и запустите. Для clock/theme/locale выберите
   соответствующую роль. Установка или запуск UI **не запускают service автоматически**.
4. `Remove` требует подтверждения. Factory identity защищена; удалить её как пользовательскую
   нельзя. Uninstall останавливает instances пакета и удаляет его app kv; для shared state
   не обещается полное стирание всех сохранённых значений — это не secure erase.

Обновление пакета останавливает его старые службы/handles и не переносит новые grants
автоматически. Review permissions после update. Несовместимые старые objects сохраняются
в storage, но могут не отображаться в executable catalog до пересборки/установки.

## Выдать и отозвать доступ

Откройте карточку → `Permissions` или отдельную вкладку разрешений и выберите пакет.
Просмотрите permission и scope, нажмите Allow/Revoke и подтвердите. Успех показывается
только после ответа Core. При storage error текущий доступ может быть уже заблокирован
в RAM, но durable revoke не подтверждён; после reboot старое право может вернуться.

Разрешение temperature не даёт humidity/pressure, а background execution не даёт sensors,
Companion, audio или network. Component должен объявлять доступ даже при package-level grant.
Persistent grant живёт до отзыва/смены generation; session не должен восстанавливаться после
disconnect/reboot. Кнопка доступна для persistent/session/selected-resource: проверяйте показанный scope.
Отдельного выбора произвольного файла/ресурса здесь нет; selected-resource ограничивается
объявленным scope и backend. Для restricted и explicit-action обычная кнопка недоступна.
Наличие имени в списке не гарантирует, что API реализован. Уже полученные/сохранённые приложением
данные отзывом не удаляются. [Точные правила SDK](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/ru/LUA-API.md).

## Службы и автозапуск

Откройте Diagnostics → «Фоновые службы». Это список реальных service components,
состояния и причины из Core, не процессы компьютера.

1. Выдайте «Фоновое выполнение» (`app.background.execute`) явно; Start сам его не выдаёт.
   Нужные sensor permissions выдаются отдельно в Ignite Hub.
2. Нажмите Start. При Running проверьте component/instance и состояние; при отказе смотрите
   reason/error: нет права, model support, low energy, memory admission или quota fault.
3. Pause отменяет активный спрос/timers; Resume восстанавливает выполнение. Stop освобождает
   instance. Revoke execution останавливает службу и запрещает новый запуск.
4. Для autostart отдельно разрешите `app.background.autostart`, затем включите switch
   `Start this service automatically`. Нужны execution и autostart grants, а также политика.
   По умолчанию всё выключено; один grant ещё не включает запуск после boot.

Закрытие UI приложения, панели Diagnostics, Preview или Flare не является Stop службы
физического устройства. Отключение прекращает мониторинг, не persistent execution.
Приостанавливаются по energy policy только по правилам Core; ручная пауза не снимается
автоматически. Fault требует явного restart после устранения причины. При неуспехе сохранения
crash latch старое autostart preference может пережить reboot; смотрите сообщения ошибок.
Сборка/host tests не подтверждают одновременные две VM на плате. [Подробности](background-services.md).

В Simulator disconnect уничтожает виртуальное устройство и его session storage; новый Simulator — новая сессия. Это отличается от отключения кабеля физических часов.

## Датчики, настройки и данные компьютера

Diagnostics → Sensors показывает metadata/configured sources. Catalog не запускает поток.
Для значений явно запустите диагностику; lease ограничен, интервал/частота ограничены.
Stop, закрытие страницы/окна и disconnect прекращают диагностический demand. Это отдельное
разрешение сессии, не grant установленного Ignite. Raw IMU ODR по BLE не передаётся по умолчанию.

В «Мои часы» → «Настройки часов»: Sync time, UTC offset (минуты), Bluetooth и доступные
outputs. Для brightness/automatic ALS/mute нужны настроенные backend; отсутствие элемента
или `not supported` не исправляется grant. RTC acknowledgement может сообщить частичный
результат: Core принял время, RTC не подтвердил. Не считайте неизвестную батарею 0% или charging.

CPU/RAM компьютера требует и app grant `companion.pc.metrics` со scope `cpu,ram`, и отдельной
локальной политики Flare, и активного потребителя. Backend CPU/RAM пока Linux-only; Windows/
macOS не получают выдуманные значения. Аутентификация и установка Ignite не включают поток.

## Troubleshooting

| Симптом | Последовательность действий |
| --- | --- |
| USB не подключается | data-кабель → порт ОС → закрыть monitor → выбрать порт/USB → проверить handshake/contract |
| BLE нет в scan | сначала USB → проверить Bluetooth enabled → открыть pairing window → scan → выбрать BLE |
| «Incompatible build/package» | сверить SDK revision firmware/factory/Flare/package; пересобрать совместимым compiler; не erase storage |
| «not supported» | возможность не реализована/модуль не сконфигурирован; grant не добавляет железо |
| Источник недоступен | configured source отсутствует/ошибается/stale; проверить state/reason/age, wiring с разработчиком |
| Нет разрешения | component declaration → точный scope → подтверждённый grant → update generation |
| Pending/not ready | дождаться conversion/deadline; не принимать nil/0 за измерение |
| Установка прервалась | reconnect → обновить каталог → проверить версию, только затем повторить/продолжить |
| Service не стартует | execution grant → model limit → energy → memory/fault reason; остановить ненужную службу вручную |
| Studio не находит PIL/Python | активировать правильное Python environment перед запуском Flare; compiler subprocess использует PATH |

Если обычные шаги не помогли, [SUPPORT](../../SUPPORT.md): версия, ОС, модель, транспорт,
точный симптом/ошибка и воспроизведение без private data. Не отправляйте pairing keys/grants.

## Recovery и firmware

При действующем соединении «Мои часы» → «Настройки часов» → `Restore factory components`
→ подтвердить. Это сброс выбранных ролей и активация factory компонентов, не format,
reflash или остановка всех служб. Службы остановите отдельно. Если factory/storage/Core
не запускается, команда может быть недоступна; [порядок OS recovery](https://github.com/DeeTech-Labs/FlinT-OS/blob/main/docs/ru/getting-started/device-use.md).
Firmware обновляется инструментами OS/ESP-IDF с согласованным factory.bin, не через Flare.
Изменение partition layout требует отдельной миграции данных; текущая интеграция его не меняла.
