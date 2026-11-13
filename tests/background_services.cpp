#include "platform_app/BackgroundServices.h"
#include "platform_app/IgniteHub.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "%d: %s\n", __LINE__, #x);                                        \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
struct PlatformClientTestAccess {
    static void metadataError(PlatformClient &client) {
        CHECK(client.pending_ && client.pending_->frame.method == flint::Method::services);
        const auto frame = client.pending_->frame;
        flint::Writer reply;
        reply.n(uint16_t(flint::Error::source_unavailable), 2);
        const auto bytes = flint::encodeFrame({frame.method, frame.id, true, reply.data});
        client.receive(QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    }
};
template <class Predicate> bool waitFor(Predicate predicate, int milliseconds = 5000) {
    QElapsedTimer clock;
    clock.start();
    while (!predicate() && clock.elapsed() < milliseconds) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return predicate();
}
static flint::Error request(PlatformClient &client, flint::Method method, flint::Bytes payload) {
    bool done = false;
    auto result = flint::Error::invalid_state;
    client.request(method, std::move(payload), [&](auto error, auto) {
        result = error;
        done = true;
    });
    CHECK(waitFor([&] { return done; }));
    return result;
}
static QVariantMap service(const BackgroundServices &background) {
    for (const auto &entry : background.services()) {
        const auto row = entry.toMap();
        if (row.value("package") == "demo.thermometer-service" &&
            row.value("component") == "worker")
            return row;
    }
    return {};
}
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName("FlintTests");
    QCoreApplication::setApplicationName("BackgroundServices");
    QTemporaryDir preferences;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    PlatformClient client;
    client.setAutoSyncTime(false);
    IgniteHub hub(&client);
    BackgroundServices background(&client);
    client.startSimulator();
    CHECK(waitFor([&] { return client.connected() && !hub.packages().empty(); }));
    CHECK(!background.panelActive() && background.services().empty());
    hub.install(QUrl::fromLocalFile(QString::fromUtf8(FLINT_THERMOMETER_PACKAGE)));
    CHECK(waitFor([&] { return !hub.busy() && hub.progress() == 1; }));
    background.setPanelActive(true);
    CHECK(waitFor([&] { return !background.busy() && !service(background).empty(); }));
    CHECK(background.policy().value("maxServices").toInt() == 2);
    CHECK(background.policy().value("memoryBytes").toUInt() == 131072);
    CHECK(service(background).value("state").toInt() == 0);
    CHECK(!service(background).value("executeGranted").toBool());
    background.control(0, "demo.thermometer-service", "worker");
    CHECK(waitFor([&] { return !background.busy(); }));
    CHECK(!service(background).value("executeGranted").toBool());
    CHECK(service(background).value("state").toInt() == 0); // Start never grants execution.
    CHECK(background.message().contains(platformError(flint::Error::permission_denied)));
    background.refresh();
    PlatformClientTestAccess::metadataError(client);
    CHECK(background.message().contains(platformError(flint::Error::permission_denied)));
    CHECK(background.message().contains(platformError(flint::Error::source_unavailable)));
    background.refresh();
    CHECK(waitFor([&] { return !background.busy() && !service(background).empty(); }));
    CHECK(background.message().contains(platformError(flint::Error::permission_denied)));
    CHECK(!background.message().contains(platformError(flint::Error::source_unavailable)));

    background.grant("demo.thermometer-service", "app.background.execute", true);
    CHECK(waitFor([&] {
        return !background.busy() && service(background).value("executeGranted").toBool();
    }));
    for (const auto &permission : {"sensors.temperature", "sensors.humidity"}) {
        flint::Writer w;
        w.text("demo.thermometer-service");
        w.text(permission);
        w.text("");
        w.n(1, 1);
        CHECK(
            request(client, flint::Method::permission_set, std::move(w.data)) == flint::Error::ok);
    }
    background.control(0, "demo.thermometer-service", "worker");
    CHECK(waitFor(
        [&] { return !background.busy() && service(background).value("state").toInt() == 2; }));
    int modelResets = 0;
    QObject::connect(
        &background, &QAbstractItemModel::modelReset, &application, [&] { ++modelResets; });
    const auto instance = service(background).value("instanceId"),
               generation = service(background).value("generation");
    const auto callbacks = service(background).value("callbacks").toULongLong();
    const auto row = service(background);
    CHECK(!row.contains("values") && !row.contains("temperature") && !row.contains("humidity") &&
        !row.contains("stateData"));
    background.setPanelActive(false);
    const auto requestCount = client.diagnostics().value("requests");
    CHECK(client.advanceSimulation(6000));
    QElapsedTimer pause;
    pause.start();
    CHECK(waitFor([&] { return pause.elapsed() >= 650; }));
    CHECK(client.diagnostics().value("requests") ==
        requestCount); // No metadata polling while hidden.
    background.setPanelActive(true);
    CHECK(waitFor(
        [&] { return !background.busy() && service(background).value("state").toInt() == 2; }));
    CHECK(service(background).value("instanceId") == instance &&
        service(background).value("generation") == generation);
    CHECK(service(background).value("callbacks").toULongLong() > callbacks);
    CHECK(modelResets == 0); // Counter refreshes preserve delegates and keyboard focus.
    hub.launch("demo.thermometer-service", "main");
    CHECK(waitFor([&] {
        return !hub.busy() && client.info().value("active") == "demo.thermometer-service";
    }));
    background.refresh();
    CHECK(waitFor([&] { return !background.busy(); }));
    CHECK(service(background).value("state").toInt() == 2);
    CHECK(service(background).value("generation") ==
        generation); // Foreground UI does not replace its service.
    client.setVirtualLowEnergy(true);
    CHECK(waitFor(
        [&] { return !background.busy() && service(background).value("state").toInt() == 3; }));
    CHECK(service(background).value("reason").toInt() == 3);
    client.setVirtualLowEnergy(false);
    CHECK(waitFor(
        [&] { return !background.busy() && service(background).value("state").toInt() == 2; }));
    background.refresh();
    CHECK(background.busy() && !background.updating());
    background.control(2, "demo.thermometer-service", "worker");
    CHECK(background.updating()); // An in-flight metadata read cannot block a user action.
    CHECK(waitFor(
        [&] { return !background.busy() && service(background).value("state").toInt() == 3; }));
    client.setVirtualLowEnergy(true);
    client.setVirtualLowEnergy(false);
    CHECK(client.advanceSimulation(200));
    background.refresh();
    CHECK(waitFor([&] { return !background.busy(); }));
    CHECK(service(background).value("state").toInt() == 3); // Manual pause remains manual.
    background.control(3, "demo.thermometer-service", "worker");
    CHECK(waitFor(
        [&] { return !background.busy() && service(background).value("state").toInt() == 2; }));
    background.grant("demo.thermometer-service", "app.background.autostart", true);
    CHECK(waitFor([&] {
        return !background.busy() && service(background).value("autostartGranted").toBool();
    }));
    CHECK(!service(background).value("autostart").toBool());
    background.control(4, "demo.thermometer-service", "worker");
    CHECK(waitFor(
        [&] { return !background.busy() && service(background).value("autostart").toBool(); }));
    background.control(6, "demo.thermometer-service", "worker");
    CHECK(waitFor([&] {
        return !background.busy() && !service(background).value("executeGranted").toBool() &&
            service(background).value("state").toInt() == 0;
    }));
    CHECK(client.advanceSimulation(10000));
    background.refresh();
    CHECK(waitFor([&] { return !background.busy(); }));
    CHECK(service(background).value("state").toInt() == 0);
    background.setPanelActive(false);
    client.disconnectDevice();
    CHECK(background.services().empty() && !background.busy());
    client.startSimulator();
    CHECK(waitFor([&] { return client.connected(); }));
    CHECK(background.message().isEmpty()); // Old-session notices never describe the new device.

    std::puts("Background service metadata, explicit grants, independent UI, hidden monitor, "
              "energy/manual pause and revoke: passed");
}
