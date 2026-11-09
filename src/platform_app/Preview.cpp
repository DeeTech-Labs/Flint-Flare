#include "Preview.h"
#include <QFile>
Preview::Preview(PlatformClient *client)
    : client_(client), cache_(std::make_unique<QTemporaryDir>()) {
    image_ = QImage(280, 240, QImage::Format_RGB32);
    image_.fill(QColor("#080d18"));
    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, &Preview::refresh);
    connect(client, &PlatformClient::simulationSceneChanged, this, [this] {
        if (pollingActive_ && !busy_)
            refresh();
    });
    connect(client, &PlatformClient::ready, this, [this] {
        if (pollingActive_) {
            if (!client_->simulated())
                timer_.start(1000);
            refresh();
        }
    });
    connect(client, &PlatformClient::disconnected, this, [this] {
        timer_.stop();
        busy_ = false;
        active_.clear();
        clearResources();
        image_.fill(QColor("#080d18"));
        ++revision_;
        emit changed();
    });
}
void Preview::refresh() {
    if (!client_->connected() || busy_)
        return;
    busy_ = true;
    if (client_->simulated()) {
        scene();
        return;
    }
    auto active = client_->info().value("active").toString();
    auto component = client_->info().value("component").toString();
    auto key = active + "/" + component + "/" + client_->info().value("generation").toString();
    if (key == active_) {
        scene();
        return;
    }
    active_ = key;
    download_.clear();
    total_ = 0;
    clearResources();
    download();
}
void Preview::download() {
    flint::Writer w;
    w.text(active_.section('/', 0, 0).toStdString());
    w.n(download_.size());
    client_->request(flint::Method::package_read, w.data, [this](auto error, auto bytes) {
        if (error != flint::Error::ok) {
            busy_ = false;
            status_ = platformError(error);
            emit changed();
            return;
        }
        flint::Reader r{bytes};
        auto total = r.n();
        if (!r.valid || total > flint::MaxPackageBytes || !total || (total_ && total_ != total) ||
            bytes.size() <= 4) {
            busy_ = false;
            active_.clear();
            return;
        }
        total_ = total;
        download_.insert(download_.end(), bytes.begin() + 4, bytes.end());
        if (download_.size() > total_) {
            busy_ = false;
            active_.clear();
            return;
        }
        if (download_.size() < total_) {
            download();
            return;
        }
        auto path = cache_->path() + "/preview.ignit";
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(reinterpret_cast<const char *>(download_.data()), download_.size()) !=
                qint64(download_.size())) {
            busy_ = false;
            return;
        }
        file.close();
        // A remote preview only loads validated resources. Lua and private state remain on
        // the watch; replaying its module here would run without its actual Core services.
        flint::Manifest manifest;
        auto error2 = resourceFile_.open(path.toStdString())
            ? resourcePackage_.open(resourceFile_.source())
            : flint::Error::io_error;
        if (error2 == flint::Error::ok)
            error2 = flint::validateApplication(resourcePackage_, manifest,
                {48 * 1024, 4 * 1024 * 1024, 4096}, client_->capabilities());
        const auto component = active_.section('/', 1, 1).toStdString();
        if (error2 == flint::Error::ok &&
            (manifest.id != active_.section('/', 0, 0).toStdString() ||
                std::none_of(manifest.components.begin(), manifest.components.end(),
                    [&](const auto &entry) { return entry.id == component; })))
            error2 = flint::Error::invalid_package;
        if (error2 == flint::Error::ok)
            resources_.bind(&resourcePackage_);
        download_.clear();
        if (error2 != flint::Error::ok) {
            clearResources();
            busy_ = false;
            active_.clear();
            status_ = platformError(error2);
            emit changed();
            return;
        }
        scene();
    });
}
void Preview::scene() {
    if (auto *core = client_->simulator()) {
        FlintRender::Scene scene;
        const auto error = core->scene(scene);
        busy_ = false;
        if (error == flint::Error::ok) {
            paint(scene);
            status_ = tr("Shared Ignite Renderer");
            ++revision_;
        } else
            status_ = platformError(error);
        emit changed();
        return;
    }
    client_->request(flint::Method::scene, {}, [this](auto error, auto bytes) {
        busy_ = false;
        if (error != flint::Error::ok) {
            status_ = platformError(error);
            emit changed();
            return;
        }
        FlintRender::Scene s;
        if (!FlintRender::decode(bytes.data(), bytes.size(), s)) {
            status_ = tr("Invalid scene");
            emit changed();
            return;
        }
        paint(s);
        status_ = tr("Shared Ignite Renderer");
        ++revision_;
        emit changed();
    });
}
void Preview::paint(const FlintRender::Scene &scene) {
    std::vector<uint16_t> pixels(280 * 240);
    FlintRender::Canvas canvas(pixels.data(), 280, 240);
    if (auto *core = client_->simulator())
        core->paint(scene, canvas);
    else {
        FlintRender::resourceContext = &resources_;
        FlintRender::resourceMetrics = [](void *c, unsigned f, uint32_t cp) {
            return static_cast<flint::ResourceManager *>(c)->metrics(f, cp);
        };
        FlintRender::resourceGlyph = [](void *c, unsigned f, uint32_t cp) {
            return static_cast<flint::ResourceManager *>(c)->glyph(f, cp);
        };
        FlintRender::resourceImage = [](void *c, FlintRender::Canvas &canvas,
                                         const FlintRender::Op &op) {
            static_cast<flint::ResourceManager *>(c)->image(canvas, op);
        };
        FlintRender::render(scene, canvas);
        FlintRender::resourceContext = nullptr;
        FlintRender::resourceGlyph = nullptr;
        FlintRender::resourceMetrics = nullptr;
        FlintRender::resourceImage = nullptr;
    }
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 280; ++x) {
            auto c = pixels[y * 280 + x];
            image_.setPixel(x, y,
                qRgb(((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31));
        }
}

bool Preview::capture(const QUrl &url) {
    return url.isLocalFile() && image_.save(url.toLocalFile());
}

void Preview::setActive(bool active) {
    if (pollingActive_ == active) {
        return;
    }
    if (!active)
        setInputFocus(false);
    pollingActive_ = active;
    if (active && client_->connected()) {
        if (!client_->simulated())
            timer_.start(1000);
        refresh();
    } else {
        timer_.stop();
    }
    emit changed();
}

void Preview::clearResources() {
    resources_.bind(nullptr);
    resourcePackage_.close();
    resourceFile_.close();
    pressedGeneration_ = 0;
}
void Preview::setInputFocus(bool focused) {
    if (!focused)
        pressedGeneration_ = 0;
    auto *core = client_->simulator();
    if (!core || !pollingActive_)
        return;
    const auto error = client_->simulatorInput(
        {focused ? flint::InputKind::focus_gained : flint::InputKind::focus_lost, 0,
            core->runtime().generation()});
    // Declarative screens and packages without input.focused legitimately reject focus events.
    if (error != flint::Error::ok && error != flint::Error::permission_denied &&
        error != flint::Error::invalid_state) {
        status_ = platformError(error);
        emit changed();
    }
}
bool Preview::sendInput(const QString &kind, int value) {
    auto *core = client_->simulator();
    if (!core || !pollingActive_)
        return false;
    flint::InputKind input;
    if (kind == "press")
        input = flint::InputKind::press;
    else if (kind == "release")
        input = flint::InputKind::release;
    else if (kind == "hold")
        input = flint::InputKind::hold;
    else if (kind == "rotate")
        input = flint::InputKind::rotate;
    else
        return false;
    const auto current = core->runtime().generation();
    if (input == flint::InputKind::press || input == flint::InputKind::rotate)
        setInputFocus(true);
    if (input == flint::InputKind::press)
        pressedGeneration_ = current;
    const auto generation = input == flint::InputKind::release || input == flint::InputKind::hold
        ? pressedGeneration_
        : current;
    const auto error = client_->simulatorInput({input, value, generation});
    if (input == flint::InputKind::release)
        pressedGeneration_ = 0;
    if (error != flint::Error::ok) {
        if (error == flint::Error::stale_handle)
            return false;
        status_ = error == flint::Error::permission_denied || error == flint::Error::invalid_state
            ? tr("This application does not accept input.")
            : platformError(error);
        emit changed();
    }
    return error == flint::Error::ok;
}
