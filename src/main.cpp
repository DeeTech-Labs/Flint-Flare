#include "localization/Localization.h"
#include "platform_app/CompanionService.h"
#include "platform_app/IgniteHub.h"
#include "platform_app/PlatformClient.h"
#include "platform_app/Preview.h"
#include "platform_app/Studio.h"
#include "services/BackgroundAgent.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <cstdio>
int main(int argc, char **argv) {
    QQuickStyle::setStyle("Basic");
    QApplication app(argc, argv);
    app.setOrganizationName("DeeTech Labs");
    app.setApplicationName("Flint Flare");
    app.setApplicationVersion(FLINT_FLARE_VERSION);
    QCommandLineParser args;
    args.addHelpOption();
    args.addVersionOption();
    args.addOption({"smoke", "Run the offline simulator/QML verification and exit."});
    args.addOption({"capture", "Save an offline UI screenshot.", "path"});
    args.process(app);
    std::unique_ptr<QTemporaryDir> testPreferences;
    if (args.isSet("smoke")) {
        testPreferences = std::make_unique<QTemporaryDir>();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, testPreferences->path());
    }
    if (!args.isSet("smoke")) {
        QSettings previous("DeeTech Labs", "FlinT Desktop");
        QSettings current;
        if (!current.value("migration/desktopSettings", false).toBool()) {
            for (const auto &key : previous.allKeys()) {
                if (!current.contains(key))
                    current.setValue(key, previous.value(key));
            }
            current.setValue("migration/desktopSettings", true);
            current.sync();
        }
    }
    Localization localization;
    BackgroundAgent backgroundAgent;
    PlatformClient client;
    IgniteHub hub(&client);
    Preview preview(&client);
    CompanionService companion(&client);
    Studio studio;
    QQmlApplicationEngine engine;
    engine.addImageProvider("ignite", new PreviewProvider(&preview));
    engine.rootContext()->setContextProperty("client", &client);
    engine.rootContext()->setContextProperty("localization", &localization);
    engine.rootContext()->setContextProperty("backgroundAgent", &backgroundAgent);
    QObject::connect(
        &localization, &Localization::languageChanged, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("hub", &hub);
    engine.rootContext()->setContextProperty("preview", &preview);
    engine.rootContext()->setContextProperty("companion", &companion);
    engine.rootContext()->setContextProperty("studio", &studio);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Flint.Flare", "Main");
    QObject::connect(&backgroundAgent, &BackgroundAgent::showWindow, &engine, [&engine] {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
            window->show();
            window->raise();
            window->requestActivate();
        }
    });
    if (args.isSet("smoke")) {
        // Keep layout captures deterministic and exercise the reduced-motion path.
        auto *theme = engine.singletonInstance<QObject *>(qmlTypeId("Flint.Flare", 1, 0, "Theme"));
        theme->setProperty("animations", false);
        theme->setProperty("mode", 2);
        localization.setLanguage("ru");
        QObject::connect(&client, &PlatformClient::failure, &app, [&](const QString &message) {
            fprintf(stderr, "%s\n", qPrintable(message));
            app.exit(2);
        });
        struct OfflineState {
            QTemporaryDir files;
            int phase = 0;
            int page = 0;
        };
        const auto state = std::make_shared<OfflineState>();
        const auto factoryCount = QDir(":/factory").entryList({"*.ignit"}, QDir::Files).size();
        auto *check = new QTimer(&app);
        check->setInterval(100);
        QObject::connect(check, &QTimer::timeout, &app, [&, state, check, theme, factoryCount] {
            if (!client.connected() && state->phase != 10)
                return;
            const QString sampleId = "example.pc-monitor";
            bool installed = false;
            for (const auto &entry : hub.packages()) {
                if (entry.toMap().value("id").toString() == sampleId)
                    installed = true;
            }
            bool consentKnown = false;
            bool consentEnabled = false;
            for (const auto &entry : hub.permissions()) {
                const auto permission = entry.toMap();
                if (permission.value("name") == "companion.pc.metrics") {
                    consentKnown = true;
                    consentEnabled = permission.value("enabled").toBool();
                }
            }
            switch (state->phase) {
            case 0: {
                if (hub.packages().size() != factoryCount || preview.status().isEmpty() ||
                    client.info().value("epoch").toULongLong() < 1700000000ULL ||
                    !client.timeSynced())
                    return;
                auto *window = engine.rootObjects().value(0);
                auto *sync = window ? window->findChild<QObject *>("syncTimeButton") : nullptr;
                if (!sync || !QMetaObject::invokeMethod(sync, "clicked") ||
                    !client.timeSyncBusy() || sync->property("enabled").toBool()) {
                    fprintf(stderr, "Overview synchronization did not expose its pending state\n");
                    app.exit(16);
                    return;
                }
                const auto path = state->files.path() + "/sample.ignit";
                if (!QFile::copy(":/samples/sample.ignit", path)) {
                    app.exit(5);
                    return;
                }
                state->phase = 1;
                hub.install(QUrl::fromLocalFile(path));
                auto *permissions = window->findChild<QObject *>("permissionsAction");
                auto *operation = window->findChild<QObject *>("hubOperationStatus");
                QMetaObject::invokeMethod(permissions, "clicked");
                const auto installMessage = hub.message();
                hub.select(hub.packages().first().toMap().value("id").toString());
                if (!hub.busy() || !operation || !operation->property("visible").toBool() ||
                    installMessage.isEmpty() || hub.message() != installMessage) {
                    fprintf(stderr, "Permissions route hid the active installation\n");
                    app.exit(17);
                    return;
                }
                window->setProperty("section", 0);
                return;
            }
            case 1:
                if (!installed || hub.busy())
                    return;
                state->phase = 2;
                hub.launch(sampleId, "main");
                return;
            case 2:
                if (client.info().value("active") != sampleId)
                    return;
                state->phase = 3;
                hub.select(sampleId);
                return;
            case 3:
                if (!consentKnown)
                    return;
                state->phase = 4;
                hub.grant("companion.pc.metrics", "cpu,ram", true);
                return;
            case 4:
                if (!consentEnabled)
                    return;
                state->phase = -1;
                {
                    flint::Writer sample;
                    sample.n(2500);
                    sample.n(5000);
                    client.request(
                        flint::Method::metrics, sample.data, [&, state](auto error, auto) {
                            if (error != flint::Error::ok) {
                                app.exit(6);
                                return;
                            }
                            state->phase = 5;
                        });
                }
                return;
            case 5:
                state->phase = 6;
                hub.grant("companion.pc.metrics", "cpu,ram", false);
                return;
            case 6:
                if (consentEnabled)
                    return;
                state->phase = 7;
                hub.remove(sampleId);
                return;
            case 7:
                if (installed || hub.packages().size() != factoryCount || hub.busy())
                    return;
                state->phase = 8;
                // Let SwipeView finish its transition before checking or capturing a page.
                check->setInterval(350);
                return;
            case 8: {
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
                auto *pages = window ? window->findChild<QObject *>("pageStack") : nullptr;
                if (!window || !pages || pages->property("count").toInt() != 7) {
                    app.exit(7);
                    return;
                }
                if (state->page == 1 || state->page == 8) {
                    auto *card = window->findChild<QQuickItem *>("overviewPreview");
                    auto *caption = card ? card->findChild<QQuickItem *>("previewStatus") : nullptr;
                    auto *back = window->findChild<QObject *>("backButton");
                    auto *connection = window->findChild<QObject *>("connectionButton");
                    if (!card || !caption || !back || !connection ||
                        back->property("accessibleName").toString().isEmpty() ||
                        connection->property("accessibleName").toString().isEmpty() ||
                        caption->mapToItem(card, QPointF(0, caption->height())).y() >
                            card->height()) {
                        fprintf(stderr, "Overview preview or accessible navigation is invalid\n");
                        app.exit(12);
                        return;
                    }
                    if (args.isSet("capture"))
                        window->grabWindow().save(args.value("capture") +
                            (state->page == 1 ? ".overview.png" : ".overview-narrow.png"));
                    auto *permissions = window->findChild<QObject *>("permissionsAction");
                    auto *hubAction = window->findChild<QObject *>("hubAction");
                    auto *gallery = window->findChild<QObject *>("igniteHubPage");
                    auto *picker = window->findChild<QObject *>("permissionApplicationPicker");
                    if (!permissions || !hubAction || !gallery || !picker ||
                        !QMetaObject::invokeMethod(permissions, "clicked") ||
                        window->property("section").toInt() != 2 ||
                        gallery->property("currentTab").toInt() != 1) {
                        fprintf(
                            stderr, "Overview permissions action did not open its destination\n");
                        app.exit(13);
                        return;
                    }
                    picker->setProperty("currentIndex", 0);
                    if (!QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, 0)) ||
                        hub.selected() != hub.packages().first().toMap().value("id").toString()) {
                        fprintf(stderr, "Permission application selection failed\n");
                        app.exit(14);
                        return;
                    }
                    auto *consent = window->findChild<QObject *>("permissionConsent");
                    if (!consent) {
                        app.exit(18);
                        return;
                    }
                    consent->setProperty("packageId", hub.selected());
                    QMetaObject::invokeMethod(consent, "open");
                    hub.select(hub.packages()[1].toMap().value("id").toString());
                    const auto requests = client.diagnostics().value("requests");
                    QMetaObject::invokeMethod(consent, "accepted");
                    if (consent->property("visible").toBool() ||
                        client.diagnostics().value("requests") != requests) {
                        fprintf(stderr, "Stale permission consent remained actionable\n");
                        app.exit(18);
                        return;
                    }
                    QMetaObject::invokeMethod(hubAction, "clicked");
                    if (gallery->property("currentTab").toInt() != 0) {
                        app.exit(15);
                        return;
                    }
                }
                if (!window->findChild<QObject *>("watchDetails") ||
                    window->findChild<QObject *>("watchEmptyState")) {
                    fprintf(stderr, "Connected watch information is missing\n");
                    app.exit(8);
                    return;
                }
                if (state->page == 2 || state->page == 9) {
                    auto *watchPage = window->findChild<QObject *>("watchPage");
                    if (!watchPage || !window->findChild<QObject *>("watchControls")) {
                        fprintf(stderr, "Watch settings failed to open\n");
                        app.exit(11);
                        return;
                    }
                    watchPage->setProperty("settingsExpanded", false);
                    if (args.isSet("capture"))
                        window->grabWindow().save(args.value("capture") +
                            (state->page == 2 ? ".watch.png" : ".watch-narrow.png"));
                }
                if (state->page == 7) {
                    theme->setProperty("mode", 1);
                    localization.setLanguage("en");
                }
                window->setWidth(state->page < 7 ? 1320 : 540);
                window->setProperty("section", state->page % 7);
                if (state->page % 7 == 1)
                    window->findChild<QObject *>("watchPage")->setProperty("settingsExpanded", true);
                if (++state->page == 14) {
                    state->phase = 9;
                    window->setWidth(1320);
                    window->setProperty("section", 0);
                }
                return;
            }
            case 9: {
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
                window->setProperty("section", 1);
                auto *consent = window->findChild<QObject *>("permissionConsent");
                consent->setProperty("packageId", hub.selected());
                QMetaObject::invokeMethod(consent, "open");
                client.disconnectDevice();
                if (consent->property("visible").toBool()) {
                    app.exit(19);
                    return;
                }
                state->phase = 10;
                return;
            }
            case 10: {
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
                auto *connectButton = window->findChild<QObject *>("watchConnectButton");
                if (!window->findChild<QObject *>("watchEmptyState") ||
                    window->findChild<QObject *>("watchDetails") || !connectButton ||
                    !client.info().isEmpty()) {
                    fprintf(stderr, "Disconnected watch still shows device information\n");
                    app.exit(9);
                    return;
                }
                if (args.isSet("capture"))
                    window->grabWindow().save(args.value("capture") +
                        (state->page == 14 ? ".watch-empty.png" : ".watch-empty-narrow.png"));
                if (state->page == 14) {
                    window->setWidth(540);
                    ++state->page;
                    return;
                }
                QMetaObject::invokeMethod(connectButton, "clicked");
                if (window->property("section").toInt() != 6) {
                    fprintf(stderr, "Watch connection action did not open connection page\n");
                    app.exit(10);
                    return;
                }
                client.startSimulator();
                theme->setProperty("mode", 2);
                localization.setLanguage("ru");
                window->setWidth(1320);
                window->setProperty("section", 0);
                state->phase = 11;
                return;
            }
            case 11: {
                check->stop();
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
                auto *atmosphere =
                    window ? window->findChild<QObject *>("heroAtmosphere") : nullptr;
                auto *clock =
                    atmosphere ? atmosphere->findChild<QObject *>("atmosphereClock") : nullptr;
                if (!clock) {
                    app.exit(20);
                    return;
                }
                const auto phase = atmosphere->property("phase").toDouble();
                theme->setProperty("animations", true);
                window->requestActivate();
                QTimer::singleShot(300, &app, [&, window, atmosphere, clock, phase, theme] {
                    const bool running = atmosphere->property("motionRunning").toBool() &&
                        atmosphere->property("gpuAvailable").toBool();
                    const bool advanced = atmosphere->property("phase").toDouble() > phase;
                    if ((running && !advanced) ||
                        (!running && clock->property("running").toBool())) {
                        fprintf(stderr, "Hero background clock did not respect its motion state\n");
                        app.exit(20);
                        return;
                    }
                    theme->setProperty("animations", false);
                    if (clock->property("running").toBool()) {
                        app.exit(20);
                        return;
                    }
                    printf(
                        "Hero background: GPU=%d, active=%d, advanced=%d; reduced motion paused\n",
                        atmosphere->property("gpuAvailable").toBool(), window->isActive(),
                        advanced);
                    if (args.isSet("capture")) {
                        if (!window || !window->grabWindow().save(args.value("capture"))) {
                            app.exit(3);
                            return;
                        }
                    }
                    printf(
                        "Offline QML, all seven pages at two widths, watch disconnect/connect "
                        "action, binary handshake, Hub "
                        "install/launch/remove, consent/revocation and shared preview: passed\n");
                    app.exit(0);
                });
                return;
            }
            default:
                return;
            }
        });
        check->start();
        QTimer::singleShot(0, &client, &PlatformClient::startSimulator);
        QTimer::singleShot(15000, &app, [&, state, factoryCount] {
            fprintf(stderr, "Offline smoke test timed out: %s; phase=%d, packages=%lld/%lld, preview=%s, synced=%d\n",
                qPrintable(client.status()), state->phase,
                static_cast<long long>(hub.packages().size()), static_cast<long long>(factoryCount),
                qPrintable(preview.status()), client.timeSynced());
            app.exit(4);
        });
    }
    return app.exec();
}
