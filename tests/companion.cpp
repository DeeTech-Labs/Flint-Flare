#include "platform_app/CompanionService.h"
#include "platform_app/IgniteHub.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <cstdio>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%d: %s\n", __LINE__, #x);                                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)
struct PlatformClientTestAccess {
    static void receive(PlatformClient &client, const flint::Frame &frame) {
        const auto bytes = flint::encodeFrame(frame);
        client.receive(QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    }
    static void radioCapability(PlatformClient &client) {
        client.capabilities_ |= flint::CapBlePolicy;
        client.refreshBlePolicy();
    }
    static void reply(PlatformClient &client, flint::Error error, flint::Bytes data = {}) {
        const auto frame = client.pending_->frame;
        flint::Writer result;
        result.n(uint16_t(error), 2);
        result.data.insert(result.data.end(), data.begin(), data.end());
        receive(client, {frame.method, frame.id, true, result.data});
    }
    static void closing(PlatformClient &client, std::function<void()> done) {
        client.disconnectDevice();
        client.closing_ = true;
        client.closeId_ = 200;
        client.afterClose_ = std::move(done);
        client.closeTimeout_.start();
    }
    static void usb(PlatformClient &client, const flint::Frame &frame) {
        const auto bytes = flint::encodeFrame(frame);
        client.receive(
            QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size()), true);
    }
    static void expireRequest(PlatformClient &client) {
        QMetaObject::invokeMethod(&client.timeout_, "timeout", Qt::DirectConnection);
    }
    static bool waiting(const PlatformClient &client) {
        return !client.queue_.empty() || (client.pending_ && client.timeout_.isActive()) ||
            (client.installPending_ && client.installTimeout_.isActive());
    }
    static bool installWaiting(const PlatformClient &client) {
        return client.installPending_ && client.installTimeout_.isActive();
    }
};
static bool waitFor(const std::function<bool()> &predicate, int milliseconds = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return predicate();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Flint Tests");
    app.setApplicationName("Companion");
    QTemporaryDir preferences;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    // Synchronization is acknowledged, deduplicated, and scoped to the connection.
    {
        PlatformClient syncClient;
        syncClient.setAutoSyncTime(false);
        syncClient.syncTime(); // Disconnected actions cannot start an operation.
        CHECK(!syncClient.timeSyncBusy() && !syncClient.timeSynced());
        syncClient.startSimulator();
        CHECK(waitFor([&] { return syncClient.connected() && !syncClient.info().isEmpty(); }));
        syncClient.syncTime();
        CHECK(syncClient.timeSyncBusy() && !syncClient.timeSynced());
        const auto requests = syncClient.diagnostics().value("requests");
        syncClient.syncTime();
        CHECK(syncClient.diagnostics().value("requests") == requests);
        PlatformClientTestAccess::reply(syncClient, flint::Error::permission_denied);
        CHECK(!syncClient.timeSyncBusy() && !syncClient.timeSynced());
        CHECK(!syncClient.timeSyncError().isEmpty());
        syncClient.syncTime();
        CHECK(syncClient.timeSyncBusy() && syncClient.timeSyncError().isEmpty());
        CHECK(waitFor([&] { return syncClient.timeSynced(); }));
        CHECK(!syncClient.timeSyncBusy() && syncClient.timeSyncError().isEmpty());
        CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(syncClient); }));
        syncClient.syncTime();
        syncClient.disconnectDevice();
        CHECK(!syncClient.timeSyncBusy() && !syncClient.timeSynced());
        syncClient.startSimulator();
        CHECK(waitFor([&] { return syncClient.connected() && !syncClient.info().isEmpty(); }));
        CHECK(!syncClient.timeSynced()); // An old simulator reply cannot confirm a new session.
        syncClient.syncTime();
        PlatformClientTestAccess::expireRequest(syncClient);
        CHECK(!syncClient.connected() && !syncClient.timeSyncBusy() && !syncClient.timeSynced());
        CHECK(syncClient.timeSyncError().isEmpty());
        // A synchronous QML/signal listener can close the session on either notification.
        syncClient.startSimulator();
        CHECK(waitFor([&] { return syncClient.connected() && !syncClient.info().isEmpty(); }));
        auto closeOnPending =
            QObject::connect(&syncClient, &PlatformClient::timeSyncChanged, &syncClient, [&] {
                if (syncClient.timeSyncBusy())
                    syncClient.disconnectDevice();
            });
        syncClient.syncTime();
        CHECK(!syncClient.connected() && syncClient.timeSyncError().isEmpty());
        CHECK(!syncClient.timeSyncBusy() && !syncClient.timeSynced());
        QObject::disconnect(closeOnPending);
        syncClient.startSimulator();
        CHECK(waitFor([&] { return syncClient.connected() && !syncClient.info().isEmpty(); }));
        int unexpectedFailures = 0;
        auto failure = QObject::connect(&syncClient, &PlatformClient::failure, &syncClient,
            [&](const QString &) { ++unexpectedFailures; });
        auto closeOnSuccess =
            QObject::connect(&syncClient, &PlatformClient::timeSyncChanged, &syncClient, [&] {
                if (syncClient.timeSynced())
                    syncClient.disconnectDevice();
            });
        syncClient.syncTime();
        CHECK(waitFor([&] { return !syncClient.connected(); }));
        CHECK(unexpectedFailures == 0 && syncClient.timeSyncError().isEmpty());
        QObject::disconnect(closeOnSuccess);
        QObject::disconnect(failure);
        syncClient.setAutoSyncTime(true);
        syncClient.startSimulator();
        CHECK(waitFor([&] { return syncClient.connected() && syncClient.timeSynced(); }));
        syncClient.disconnectDevice();
        CHECK(!syncClient.timeSynced());
    }
    {
        PlatformClient permissionClient;
        permissionClient.setAutoSyncTime(false);
        IgniteHub hub(&permissionClient);
        permissionClient.startSimulator();
        CHECK(waitFor([&] {
            return hub.packages().size() >= 2 &&
                !PlatformClientTestAccess::waiting(permissionClient);
        }));
        const auto first = hub.packages()[0].toMap().value("id").toString();
        const auto second = hub.packages()[1].toMap().value("id").toString();
        hub.select(first);
        hub.select(second);
        hub.select(first);
        flint::Writer stale;
        stale.n(1, 2);
        stale.text("stale.permission");
        stale.text("");
        stale.n(1, 1);
        stale.n(1, 1);
        PlatformClientTestAccess::reply(permissionClient, flint::Error::ok, stale.data);
        CHECK(hub.selected() == first && hub.permissions().isEmpty());
        CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(permissionClient); }));
        for (const auto &permission : hub.permissions())
            CHECK(permission.toMap().value("name") != "stale.permission");
        const auto operationMessage = hub.message();
        hub.select(first);
        PlatformClientTestAccess::reply(permissionClient, flint::Error::permission_denied);
        CHECK(!hub.permissionsMessage().isEmpty() && hub.message() == operationMessage);
        hub.select(second);
        CHECK(hub.permissionsMessage().isEmpty());
        CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(permissionClient); }));
        hub.grant("test.permission", "", true);
        hub.select(first);
        PlatformClientTestAccess::reply(permissionClient, flint::Error::permission_denied);
        CHECK(hub.permissionsMessage().isEmpty()); // Result belongs to the previous selection.
        CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(permissionClient); }));
        // A package removed by a refreshed catalog cannot remain the permission target.
        hub.select("missing.application");
        hub.refresh();
        CHECK(waitFor([&] { return hub.selected().isEmpty(); }));
        CHECK(hub.permissions().isEmpty());
        permissionClient.disconnectDevice();
    }
    PlatformClient client;
    CompanionService companion(&client);
    client.setAutoSyncTime(false);
    client.startSimulator();
    CHECK(waitFor([&] { return client.connected() && !client.info().isEmpty(); }));
    CHECK(!companion.metricsAllowed());
    CHECK(companion.diagnostics().value("sample_attempts").toULongLong() == 0);
    companion.setMetricsAllowed(true);
    CHECK(waitFor([&] {
        return companion.status().contains("consumer") || companion.status().contains("Linux");
    }));
    CHECK(companion.diagnostics().value("permission_requests").toULongLong() == 0);
    CHECK(companion.diagnostics().value("sample_attempts").toULongLong() == 0);
    bool replied = false;
    client.request(
        flint::Method::info, {}, [&](auto error, auto) { replied = error == flint::Error::ok; });
    CHECK(PlatformClientTestAccess::waiting(client));
    flint::Writer snapshot;
    snapshot.n(flint::CompanionDemandVersion, 2);
    snapshot.n(1, 1);
    snapshot.n(333, 8);
    snapshot.n(0, 1);
    snapshot.n(1, 8);
    snapshot.n(flint::CompanionSampleIntervalMs);
    snapshot.n(flint::CompanionTtlMs);
    PlatformClientTestAccess::receive(
        client, {flint::Method::companion_state, 0, false, snapshot.data});
    CHECK(!replied && PlatformClientTestAccess::waiting(client));
    CHECK(waitFor([&] { return replied; }));
    const auto staging = QString::fromStdString(client.simulator()->packages().staging());
    CHECK(QFile::copy(QStringLiteral(FLINT_MONITOR_PACKAGE), staging));
    std::string monitor;
    CHECK(
        client.simulator()->packages().commit(staging.toStdString(), monitor) == flint::Error::ok);
    CHECK(client.simulator()->launch(monitor, "main") == flint::Error::ok);
    flint::Writer permission;
    permission.text(monitor);
    permission.text("companion.pc.metrics");
    permission.text("cpu,ram");
    permission.n(1, 1);
    bool granted = false;
    client.request(flint::Method::permission_set, permission.data,
        [&](auto error, auto) { granted = error == flint::Error::ok; });
    CHECK(waitFor([&] { return granted; }));
#ifdef Q_OS_LINUX
    CHECK(waitFor(
        [&] { return companion.diagnostics().value("metric_samples").toULongLong() > 0; }, 6000));
#else
    CHECK(companion.diagnostics().value("sample_attempts").toULongLong() == 0);
#endif
    companion.setMetricsAllowed(false);
    const auto stoppedSamples = companion.diagnostics().value("sample_attempts").toULongLong();
    CHECK(waitFor([&] { return !PlatformClientTestAccess::waiting(client); }));
    CHECK(companion.diagnostics().value("sample_attempts").toULongLong() == stoppedSamples);
    // The simulator must deliver an install reply from a later Core tick.
    QFile package(QStringLiteral(FLINT_MONITOR_PACKAGE));
    CHECK(package.open(QIODevice::ReadOnly));
    const auto bytes = package.readAll();
    package.close();
    CHECK(QFile::copy(QStringLiteral(FLINT_MONITOR_PACKAGE), staging));
    const auto metadata = std::to_string(bytes.size()) + ":" +
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toStdString();
    CHECK(flint::writeAtomic(staging.toStdString() + ".meta",
        {reinterpret_cast<const uint8_t *>(metadata.data()), metadata.size()}));
    bool committed = false;
    bool concurrentInfo = false, installSawInfo = false, independentTimeout = false;
    flint::Error commitError = flint::Error::ok;
    client.request(flint::Method::install_commit, {}, [&](auto error, auto) {
        committed = true;
        installSawInfo = concurrentInfo;
        commitError = error;
    });
    client.request(flint::Method::info, {}, [&](auto error, auto) {
        concurrentInfo = error == flint::Error::ok;
        independentTimeout = PlatformClientTestAccess::installWaiting(client);
    });
    PlatformClientTestAccess::receive(
        client, {flint::Method::companion_state, 0, false, snapshot.data});
    CHECK(!committed && PlatformClientTestAccess::waiting(client));
    CHECK(waitFor([&] { return committed && concurrentInfo; }));
    CHECK(installSawInfo && independentTimeout);
    CHECK(commitError ==
        flint::Error::incompatible_contract); // Existing version, not a second activation.
    {
        auto temporary = std::make_unique<CompanionService>(&client);
        temporary->setMetricsAllowed(true);
        temporary.reset(); // Queued callbacks must also respect QObject lifetime.
        QCoreApplication::processEvents();
    }
    // Late simulator callbacks must never complete requests from a new connection.
    bool late = false;
    client.request(flint::Method::info, {}, [&](auto, auto) { late = true; });
    client.disconnectDevice();
    QCoreApplication::processEvents();
    CHECK(!late && !companion.metricsAllowed());
    // Diagnostics count the service lifetime, including legitimate Linux samples above.
    CHECK(companion.diagnostics().value("sample_attempts").toULongLong() == stoppedSamples);
    client.startSimulator();
    CHECK(waitFor([&] { return client.connected(); }));
    CHECK(!late && !companion.metricsAllowed());
    bool quietIntervalElapsed = false;
    QTimer::singleShot(
        flint::CompanionSampleIntervalMs + 50, &app, [&] { quietIntervalElapsed = true; });
    CHECK(waitFor([&] { return quietIntervalElapsed; }, flint::CompanionSampleIntervalMs + 1000));
    CHECK(companion.diagnostics().value("sample_attempts").toULongLong() == stoppedSamples);
    // Only negotiated policy is exposed. Settings are loaded once and reflect confirmed replies.
    CHECK(!client.blePolicySupported());
    PlatformClientTestAccess::radioCapability(client);
    CHECK(waitFor([&] { return client.blePolicyKnown(); }));
    CHECK(client.bleEnabled());
    client.setBleEnabled(false);
    CHECK(client.blePolicyBusy() && client.bleEnabled());
    PlatformClientTestAccess::reply(client, flint::Error::ok);
    CHECK(!client.blePolicyBusy() && !client.bleEnabled());
    QCoreApplication::processEvents();
    client.setBleEnabled(true);
    CHECK(client.blePolicyBusy());
    PlatformClientTestAccess::reply(client, flint::Error::resource_exhausted);
    CHECK(!client.blePolicyBusy() && !client.bleEnabled());
    QCoreApplication::processEvents();
    bool closed = false;
    PlatformClientTestAccess::closing(client, [&] { closed = true; });
    PlatformClientTestAccess::usb(client, {flint::Method::info, 200, true, {0, 0}});
    PlatformClientTestAccess::usb(client, {flint::Method::session_close, 199, true, {0, 0}});
    CHECK(!closed);
    PlatformClientTestAccess::usb(client, {flint::Method::session_close, 200, true, {0, 0}});
    CHECK(closed && client.diagnostics().value("session_close_confirmed").toULongLong() == 1);
    closed = false;
    PlatformClientTestAccess::closing(client, [&] { closed = true; });
    CHECK(waitFor([&] { return closed; }));
    CHECK(client.diagnostics().value("session_close_timeouts").toULongLong() == 1);
    puts("Companion demand, unsolicited events, pending requests and stale callbacks: passed");
}
