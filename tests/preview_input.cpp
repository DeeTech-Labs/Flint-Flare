#include "platform_app/PlatformClient.h"
#include "platform_app/Preview.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%d: %s\n", __LINE__, #x);                                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)
static bool waitFor(const std::function<bool()> &predicate, int milliseconds = 3000) {
    QEventLoop loop;
    QTimer poll, timeout;
    bool ready = predicate();
    if (ready)
        return true;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if ((ready = predicate()))
            loop.quit();
    });
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start();
    timeout.start(milliseconds);
    loop.exec();
    return ready;
}
static bool contains(PlatformClient &client, std::string_view expected) {
    auto *core = client.simulator();
    if (!core)
        return false;
    FlintRender::Scene scene;
    if (core->scene(scene) != flint::Error::ok)
        return false;
    return std::string_view(reinterpret_cast<const char *>(scene.data), scene.used)
               .find(expected) != std::string_view::npos;
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("Flint Tests");
    app.setApplicationName("Preview Input");
    QTemporaryDir preferences;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    PlatformClient client;
    Preview preview(&client);
    client.startSimulator();
    CHECK(waitFor([&] { return client.connected() && !client.info().isEmpty(); }));
    CHECK(client.capabilities() & flint::CapAppApi);
    CHECK(!preview.sendInput("press", 1)); // Factory declarative clock.
    const auto staging = QString::fromStdString(client.simulator()->packages().staging());
    CHECK(QFile::copy(QStringLiteral(FLINT_COUNTER_PACKAGE), staging));
    std::string id;
    CHECK(client.simulator()->packages().commit(staging.toStdString(), id) == flint::Error::ok);
    auto launch = [&] {
        flint::Writer writer;
        writer.text(id);
        writer.text("main");
        bool done = false;
        flint::Error result = flint::Error::invalid_state;
        client.request(flint::Method::launch, writer.data, [&](auto error, auto) {
            result = error;
            done = true;
        });
        return waitFor([&] { return done; }) && result == flint::Error::ok;
    };
    CHECK(launch());
    CHECK(waitFor([&] { return contains(client, "Count: 0"); }));
    const auto oldGeneration = client.simulator()->runtime().generation();
    const auto oldRevision = preview.revision();
    CHECK(preview.sendInput("press", 1));
    CHECK(preview.sendInput("release", 0));
    CHECK(
        waitFor([&] { return contains(client, "Count: 1") && preview.revision() > oldRevision; }));
    CHECK(contains(client, "Saved"));
    CHECK(preview.sendInput("press", 1));
    CHECK(preview.sendInput("release", 0));
    CHECK(waitFor([&] { return contains(client, "Count: 2") && contains(client, "Saving..."); }));
    CHECK(waitFor(
        [&] { return contains(client, "Saved"); })); // Real Qt deadline, not direct Lua.update.
    CHECK(client.simulator()->runtime().nextWake() == UINT64_MAX);
    CHECK(launch());
    CHECK(contains(client, "Count: 2"));
    CHECK(client.simulatorInput({flint::InputKind::press, 1, oldGeneration}) ==
        flint::Error::stale_handle);
    CHECK(!preview.sendInput("hold", 1)); // No press belongs to the new run.
    CHECK(!preview.sendInput("system", 1));
    preview.setActive(false);
    CHECK(!preview.sendInput("press", 1));
    preview.setActive(true);
    CHECK(preview.sendInput("press", 1));
    CHECK(waitFor([&] { return contains(client, "Count: 3"); }));
    CHECK(client.simulator()->lastLogicError() == flint::Error::ok);
    client.disconnectDevice();
    CHECK(!preview.sendInput("press", 1));
    CHECK(client.simulatorInput({flint::InputKind::press, 1, 1}) == flint::Error::not_supported);
    puts("Qt preview input, generation, private state, scheduled save and disconnect: passed");
}
