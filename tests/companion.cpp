#include "platform_app/CompanionService.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
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
    static bool waiting(const PlatformClient &client) {
        return client.pending_ && client.timeout_.isActive();
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
    CHECK(companion.diagnostics().value("sample_attempts").toULongLong() == 0);
    client.startSimulator();
    CHECK(waitFor([&] { return client.connected(); }));
    CHECK(!late && !companion.metricsAllowed());
    puts("Companion demand, unsolicited events, pending requests and stale callbacks: passed");
}
