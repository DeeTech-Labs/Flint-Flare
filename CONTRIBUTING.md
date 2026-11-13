[English translation](docs/en/CONTRIBUTING.md)

# Вклад в Flint Flare

## Подготовка

Для первого изменения начните с [README](README.md) и [сборки](docs/ru/BUILD.md); [архитектуру](docs/ru/ARCHITECTURE.md), [Companion](docs/ru/COMPANION-PROTOCOL.md) и [changelog](CHANGELOG.md) читайте по затронутой подсистеме. Вопросы обсуждаются в [Discussions](https://github.com/DeeTech-Labs/Flint-Flare/discussions), воспроизводимые ошибки и функции — в [Issues](https://github.com/DeeTech-Labs/Flint-Flare/issues). [Маршруты поддержки](SUPPORT.md).

Требуются Qt 6.5+ с Quick, Quick Controls, Quick Dialogs, Serial Port, Bluetooth, Widgets и Shader Tools, CMake 3.21+, C++20, Python 3 и Pillow. Сборка и тесты описаны в [руководстве](docs/ru/BUILD.md). CI использует Qt 6.10.2 на Linux, macOS и Windows.

## Общий SDK и организация кода

Перед сборкой выполните `git submodule update --init --recursive`. `platform/` — закреплённый подмодуль IgniteSDK. Общие изменения вносятся в SDK, проверяются там, затем подключаются обновлением коммита подмодуля в OS и Flare. Экспорта или копирования исходников нет.

В SDK заводские приложения находятся в `system/`. Генерация из корня SDK: `python3 tools/generate_registry.py` и `python3 tools/build_system.py`. Не запускайте удалённые инструменты экспорта.

QML отвечает за представление, службы Flare — за поведение, OS — за аппаратные адаптеры. Сохраняйте библиотеку визуальных компонентов. Ввод-вывод должен быть асинхронным; проверяйте полные ответы, фрагментацию и границы. Возможности берутся из INFO, а не жёстко заданного имени модели. Не добавляйте текстовые обходы бинарного протокола. Изменения контрактов документируются и проверяются в обоих потребителях.

## Проверка

Ниже независимая сборка SDK из корня потребителя; для обычного QML/controller изменения нужен основной Flare build/CTest по BUILD, а не только эти SDK tests:

```sh
cmake -S platform -B platform/build/docs-sdk -DCMAKE_BUILD_TYPE=Debug
cmake --build platform/build/docs-sdk --config Debug
ctest --test-dir platform/build/docs-sdk -C Debug --output-on-failure
```

Для OS дополнительно `python3 -m unittest discover -s tests/tooling -v`; для Flare — CTest основной сборки и тесты каталогов локализации. Обычные тесты автономны; проверки физического USB/BLE запускаются отдельно и явно. Не добавляйте реальные учётные данные, каталоги сборки, аппаратные захваты и личные настройки Qt Creator.

## Изменения и оформление

Создавайте небольшие PR из актуальной `main`, используя ветки `feature/…`, `fix/…`, `chore/…`, `docs/…`; напрямую в `main` не отправляйте. Объясните поведение, совместимость и проверку. Отступы — четыре пробела без табуляции; стиль в `.editorconfig` и `.clang-format`. Сообщения коммитов — английские Conventional Commits в повелительной форме, например `fix(session): Reject incomplete status snapshots`.

Русский — канонический язык файловой документации. Обновляйте оригиналы и английские переводы с сохранением имён документов и рабочих ссылок. Исторические журналы и сторонние лицензии сохраняют исходный язык.

## Приложения и лицензии

Независимые Ignite создаются отдельными проектами общим компилятором, без нативной регистрации циферблатов в Core. Разрешения объявляются с точной областью; неподдерживаемые API не рекламируются. Автор сохраняет права на собственный вклад. Вклад предоставляется по лицензии соответствующего компонента; если прав недостаточно, сообщите до включения. Передача авторских прав и общее согласие на коммерческое перелицензирование не предполагаются.

[Карта лицензий](docs/ru/LICENSING.md), [LICENSE](LICENSE), [NOTICE](NOTICE). Участие регулируется [правилами сообщества](CODE_OF_CONDUCT.md).

## Небольшое изменение и проверки по подсистемам

Достаточно [собрать Flare](docs/ru/BUILD.md), открыть Simulator и найти controller по
[карте разработки](docs/ru/DEVELOPMENT.md); не нужно читать все форматы перед правкой QML.
Pillow нужен в CMake interpreter до configure; на Linux требуется QtDBus. Для MSVC build
используйте --config Debug, для CTest -C Debug; не смешивайте MinGW/MSVC Qt kits.
Все build/temp artifacts — внутри owning build/ и temp/, независимый SDK — platform/build/.

Базовые проверки: diff check, основной CMake build/CTest, RU/EN и рабочие ссылки.
QML/layout — desktop_offline и wide/narrow captures; session/RPC — companion_sessions;
Preview — preview_application_api; Studio — studio_icon_resources; sensors — sensor_sessions;
services — background_service_sessions/background_service_ui. Имена текущей конфигурации
покажет ctest -N. Shared contract changes требуют SDK tests и OS consumer build, а не
добавления другой реализации в Flare. Новые источники данных не должны включаться от auth.

Issue: ОС/Qt/compiler, transport, Flare/SDK/firmware revision, шаги, ожидаемый/фактический
результат, минимальный безопасный проект. Не публикуйте grants/pairing keys/private state.
PR: проблема и итоговое поведение, compatibility/migration/security, реально выполненные
команды, captures при UI-правке, Unreleased changelog и непроверенные платформы/железо.
Simulator/CTest не подтверждает physical BLE bonding, USB timing, power loss или батарею.
