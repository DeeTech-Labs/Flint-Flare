#include "platform_app/Sensors.h"
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
            std::fprintf(stderr, "CHECK failed at %d: %s\n", __LINE__, #x);                        \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
struct PlatformClientTestAccess {
    static void reply(
        PlatformClient &client, flint::Bytes data = {}, flint::Error error = flint::Error::ok) {
        CHECK(client.pending_);
        const auto frame = client.pending_->frame;
        flint::Writer result;
        result.n(uint16_t(error), 2);
        result.raw(data);
        const auto wire = flint::encodeFrame({frame.method, frame.id, true, result.data});
        client.receive(QByteArray(reinterpret_cast<const char *>(wire.data()), wire.size()));
    }
    static void partialSettingFailure(PlatformClient &client) {
        CHECK(client.pending_ && client.pending_->frame.method == flint::Method::settings_set);
        const auto response = client.core_->process(client.pending_->frame, client.session_);
        CHECK(
            response && response->payload.size() >= 2 && flint::u16(response->payload.data()) == 0);
        reply(client, {}, flint::Error::io_error);
    }
    static void pollTime(PlatformClient &client) {
        client.pollTimeStatus(client.generation_, client.timeSyncOperation_);
    }
    static void expireTime(PlatformClient &client) {
        QMetaObject::invokeMethod(&client.timeSyncTimeout_, "timeout", Qt::DirectConnection);
    }
    static bool waiting(PlatformClient &client) {
        return client.pending_ || !client.queue_.empty();
    }
};
template <class Predicate> bool waitFor(Predicate predicate, int timeout = 4000) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (!predicate() && elapsed.elapsed() < timeout) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return predicate();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("FlintSensorsTests");
    QCoreApplication::setApplicationName("FlintSensorsTests");
    QTemporaryDir preferences;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    PlatformClient client;
    client.setAutoSyncTime(false);
    Sensors sensors(&client);
    client.startSimulator();
    CHECK(waitFor([&] { return client.connected(); }));
    CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(client); }));
    auto timeReply = [](int state, flint::Error reason = flint::Error::ok) {
        flint::Writer w;
        w.n(1800000000, 8);
        w.n(state, 1);
        w.n(uint16_t(reason), 2);
        return w.data;
    };
    client.syncTime();
    PlatformClientTestAccess::reply(client);
    PlatformClientTestAccess::reply(client, timeReply(1));
    CHECK(client.timeSyncBusy() && !client.timeSynced());
    PlatformClientTestAccess::pollTime(client);
    PlatformClientTestAccess::reply(client, timeReply(3, flint::Error::io_error));
    CHECK(!client.timeSyncBusy() && !client.timeSynced() && client.timeSyncError().contains("RTC"));
    client.syncTime();
    PlatformClientTestAccess::reply(client);
    PlatformClientTestAccess::reply(client, timeReply(2));
    CHECK(client.timeSynced() && !client.timeSyncBusy());
    CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(client); }));
    client.syncTime();
    PlatformClientTestAccess::reply(client);
    PlatformClientTestAccess::reply(client, {0});
    CHECK(!client.timeSynced() && !client.timeSyncError().isEmpty());
    client.syncTime();
    PlatformClientTestAccess::expireTime(client);
    CHECK(!client.timeSyncBusy() && !client.timeSynced() && !client.timeSyncError().isEmpty());
    CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(client); }));
    CHECK(!client.timeSynced()); // A late reply cannot turn timeout into success.
    CHECK(!sensors.enabled() && sensors.sources().empty());
    sensors.setPanelActive(true);
    CHECK(waitFor([&] { return !sensors.busy() && sensors.sources().size() == 4; }));
    for (const auto &source : sensors.sources()) {
        CHECK(source.toMap().value("simulation").toBool());
        CHECK(source.toMap().value("sampleId").toULongLong() == 0);
    }
    CHECK(client.advanceSimulation(10000));
    sensors.refresh();
    CHECK(waitFor([&] { return !sensors.busy(); }));
    for (const auto &source : sensors.sources())
        CHECK(source.toMap().value("sampleId").toULongLong() == 0);
    CHECK(waitFor([&] {
        return !sensors.settingsBusy() && sensors.settings().value("brightness_supported").toBool();
    }));
    CHECK(sensors.settings().value("mute_supported").toBool());
    const auto originalLevel = sensors.settings().value("brightness_manual");
    sensors.setSetting("brightness_manual", "35");
    CHECK(sensors.settingsBusy() && sensors.settings().value("brightness_manual") == originalLevel);
    CHECK(waitFor([&] {
        return !sensors.settingsBusy() &&
            sensors.settings().value("brightness_manual").toString() == "35";
    }));
    CHECK(sensors.settings().value("audio_muted").toString() == "0");
    CHECK(sensors.settings().value("brightness_mode").toString() == "manual");
    CHECK(sensors.settings().value("brightness_fallback").toString() == "50");
    CHECK(client.advanceSimulation(1000));
    CHECK(waitFor([&] { return client.virtualBrightness() == 35; }));
    sensors.setSetting("brightness_manual", "4");
    CHECK(!sensors.settingsBusy() &&
        sensors.settings().value("brightness_manual").toString() == "35");
    sensors.setSetting("audio_muted", "1");
    CHECK(waitFor([&] {
        return !sensors.settingsBusy() && sensors.settings().value("audio_muted").toString() == "1";
    }));
    CHECK(client.virtualMuted());
    sensors.setSetting("audio_muted", "0");
    PlatformClientTestAccess::partialSettingFailure(client);
    CHECK(sensors.settingsBusy());
    CHECK(sensors.settings().value("audio_muted").toString() == "1");
    CHECK(waitFor([&] { return !sensors.settingsBusy(); }));
    CHECK(sensors.settings().value("audio_muted").toString() == "0" && !client.virtualMuted());
    CHECK(sensors.settingsMessage().contains(platformError(flint::Error::io_error)));
    CHECK(sensors.settingsMessage() != "Settings confirmed by device");
    sensors.setSetting("audio_muted", "1");
    PlatformClientTestAccess::partialSettingFailure(client);
    PlatformClientTestAccess::reply(client, {}, flint::Error::source_unavailable);
    CHECK(!sensors.settingsBusy() && sensors.settings().empty());
    CHECK(sensors.settingsMessage().contains(platformError(flint::Error::io_error)));
    CHECK(sensors.settingsMessage().contains(platformError(flint::Error::source_unavailable)));
    sensors.refreshSettings();
    CHECK(waitFor([&] { return !sensors.settingsBusy(); }));
    CHECK(sensors.settings().value("audio_muted").toString() == "1" && client.virtualMuted());

    sensors.setSetting("brightness_mode", "automatic");
    CHECK(waitFor([&] {
        return !sensors.settingsBusy() &&
            sensors.settings().value("brightness_mode").toString() == "automatic";
    }));
    CHECK(client.advanceSimulation(1000));
    sensors.refresh();
    CHECK(waitFor([&] { return !sensors.busy(); }));
    CHECK(sensors.sources().at(1).toMap().value("sampleId").toULongLong() > 0);
    sensors.setSetting("brightness_mode", "manual");
    CHECK(waitFor([&] {
        return !sensors.settingsBusy() &&
            sensors.settings().value("brightness_mode").toString() == "manual";
    }));
    sensors.setSetting("brightness_manual", "42");
    PlatformClientTestAccess::reply(client, {0}); // Malformed ack must not turn into success.
    CHECK(waitFor([&] { return !sensors.settingsBusy(); }));
    CHECK(!sensors.settingsMessage().isEmpty());
    CHECK(sensors.settings().value("brightness_manual").toString() == "35");
    sensors.start(1);
    CHECK(waitFor([&] { return sensors.reading().value("status").toInt() == 2; }));
    CHECK(sensors.enabled());
    CHECK(sensors.reading().value("values").toList().at(0).toMap().value("x").toDouble() == 22.0);
    const auto generation = sensors.reading().value("generation").toULongLong();
    for (int state : {1, 5, 3, 0}) {
        CHECK(client.setVirtualSensorState(1, state));
        CHECK(waitFor([&] {
            return sensors.reading().value("source").toInt() == 1 &&
                sensors.reading().value("status").toInt() == state;
        }));
        CHECK(sensors.reading().value("channels").toInt() == 0);
        CHECK(sensors.reading().value("values").toList().empty());
    }
    CHECK(client.setVirtualSensorState(1, 2));
    CHECK(waitFor([&] { return sensors.reading().value("status").toInt() == 2; }));
    CHECK(client.setVirtualSensorValue(1, 0, 27.5));
    CHECK(!client.setVirtualSensorValue(1, 3, 1));
    CHECK(!client.setVirtualSensorAvailable(900, false));
    CHECK(client.advanceSimulation(1000));
    CHECK(waitFor([&] {
        const auto values = sensors.reading().value("values").toList();
        return !values.empty() && values.at(0).toMap().value("x").toDouble() == 27.5;
    }));
    CHECK(client.resetVirtualSensor(1));
    CHECK(
        waitFor([&] { return sensors.reading().value("generation").toULongLong() > generation; }));
    CHECK(client.setVirtualSensorAvailable(1, false));
    CHECK(client.advanceSimulation(1000));
    CHECK(waitFor([&] { return sensors.reading().value("status").toInt() == 4; }));
    sensors.setPanelActive(false);
    CHECK(!sensors.enabled() && sensors.reading().empty());
    CHECK(client.advanceSimulation(10000));
    sensors.setPanelActive(true);
    CHECK(waitFor([&] { return !sensors.busy() && sensors.sources().size() == 4; }));
    CHECK(!sensors.enabled());
    const auto stoppedId = sensors.sources().at(0).toMap().value("sampleId");
    CHECK(client.advanceSimulation(10000));
    sensors.refresh();
    CHECK(waitFor([&] { return !sensors.busy(); }));
    CHECK(sensors.sources().at(0).toMap().value("sampleId") == stoppedId);
    sensors.start(2);
    CHECK(waitFor(
        [&] { return sensors.enabled() && sensors.reading().value("source").toInt() == 2; }));
    CHECK(client.advanceSimulation(31000));
    CHECK(waitFor([&] { return !sensors.enabled(); }));
    CHECK(sensors.reading().empty());
    client.disconnectDevice();
    CHECK(waitFor([&] { return !client.connected(); }));
    CHECK(!sensors.enabled() && sensors.sources().empty());
    client.startSimulator();
    CHECK(waitFor(
        [&] { return client.connected() && sensors.sources().size() == 4 && !sensors.busy(); }));
    CHECK(!sensors.enabled());
    CHECK(sensors.reading().empty());
    std::puts("Sensor catalog, explicit lease, simulation values/reset/failure, stop, expiry and "
              "reconnect: passed");
}
