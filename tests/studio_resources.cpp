#include "platform_app/Studio.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QTimer>
#include <flint/resources.h>
#include <flint/storage.h>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    if (argc != 2)
        return 1;
    QTemporaryDir directory;
    if (!directory.isValid())
        return 1;
    Studio studio;
    const auto output = directory.filePath("icons.ignit");
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&studio, &Studio::changed, &loop, [&] {
        if (!studio.busy())
            loop.quit();
    });
    studio.build(QUrl::fromLocalFile(QString::fromUtf8(argv[1])), QUrl::fromLocalFile(output));
    if (studio.busy()) {
        timeout.start(15000);
        loop.exec();
    }
    if (studio.busy() || !QFileInfo::exists(output)) {
        std::cerr << studio.message().toStdString() << '\n';
        return 1;
    }
    flint::File file;
    flint::Package package;
    flint::Manifest manifest;
    if (!file.open(output.toStdString()) || package.open(file.source()) != flint::Error::ok ||
        flint::loadManifest(package, manifest) != flint::Error::ok || !manifest.icons.main)
        return 1;
    flint::ResourceManager resources;
    resources.bind(&package);
    flint::Package::View icon;
    if (package.view(manifest.icons.main, icon) != flint::Error::ok ||
        resources.decoder().open(flint::imageSource(icon)) != flint::Error::ok ||
        resources.decoder().validate() != flint::Error::ok)
        return 1;
    std::cout << "Bundled Studio compiler built and validated real icon resources\n";
}
