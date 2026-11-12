#include "IgniteHub.h"
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryFile>
#include <algorithm>
IgniteHub::IgniteHub(PlatformClient *client) : client_(client) {
    connect(client, &PlatformClient::ready, this, &IgniteHub::refresh);
    connect(client, &PlatformClient::disconnected, this, [this] {
        busy_ = false;
        install_.clear();
        offset_ = 0;
        progress_ = 0;
        packages_.clear();
        permissions_.clear();
        selected_.clear();
        permissionsMessage_.clear();
        ++permissionRequest_;
        emit changed();
    });
}
QString IgniteHub::permissionLabel(const QString &name) const {
    if (name == QLatin1String("sensors.motion"))
        return tr("Motion sensor");
    if (name == QLatin1String("sensors.ambient_light"))
        return tr("Ambient light");
    if (name == QLatin1String("sensors.temperature"))
        return tr("Temperature");
    if (name == QLatin1String("sensors.pressure"))
        return tr("Atmospheric pressure");
    if (name == QLatin1String("sensors.humidity"))
        return tr("Humidity");
    if (name == QLatin1String("system.power.status"))
        return tr("Battery and power status");
    if (name == QLatin1String("audio.play"))
        return tr("Audio playback");
    if (name == QLatin1String("ui.brightness"))
        return tr("Display brightness");
    return name;
}
void IgniteHub::result(flint::Error e) {
    busy_ = false;
    message_ = platformError(e);
    emit changed();
    if (e == flint::Error::ok) {
        refresh();
        client_->refreshInfo();
        emit packagesChanged();
    }
}
void IgniteHub::refresh() {
    if (!client_->connected() || busy_)
        return;
    incoming_.clear();
    page(0, ++catalogRequest_);
}
void IgniteHub::page(uint32_t offset, uint64_t generation) {
    flint::Writer w;
    w.n(offset);
    client_->request(
        flint::Method::packages, w.data, [this, offset, generation](auto error, auto bytes) {
            if (generation != catalogRequest_)
                return;
            if (error != flint::Error::ok) {
                message_ = platformError(error);
                emit changed();
                return;
            }
            flint::Reader r{bytes};
            auto total = r.n(), count = r.n(2);
            if (total > 100000 || count > 128) {
                message_ = "Некорректный реестр";
                emit changed();
                return;
            }
            for (unsigned i = 0; i < count; ++i) {
                QVariantMap p;
                p["id"] = QString::fromStdString(r.text(64));
                p["name"] = QString::fromStdString(r.text(96));
                p["publisher"] = QString::fromStdString(r.text(64));
                p["version"] = qulonglong(r.n());
                p["factory"] = bool(r.n(1));
                auto componentCount = r.n(2);
                if (componentCount > 32) {
                    message_ = "Некорректный компонент";
                    emit changed();
                    return;
                }
                QVariantList components;
                for (unsigned j = 0; j < componentCount; ++j) {
                    QVariantMap c;
                    c["id"] = QString::fromStdString(r.text(64));
                    c["type"] = int(r.n(2));
                    c["role"] = int(r.n(1));
                    components.append(c);
                }
                p["components"] = components;
                incoming_.append(p);
            }
            if (!r.done() || (!count && offset < total)) {
                message_ = "Некорректный реестр";
                emit changed();
                return;
            }
            if (offset + count < total) {
                page(offset + count, generation);
                return;
            }
            packages_ = incoming_;
            if (!std::any_of(packages_.cbegin(), packages_.cend(), [this](const auto &entry) {
                    return entry.toMap().value("id").toString() == selected_;
                })) {
                selected_.clear();
                permissionsMessage_.clear();
                permissions_.clear();
                ++permissionRequest_;
            }
            emit changed();
            if (!selected_.isEmpty())
                permissionsRefresh();
        });
}
void IgniteHub::select(const QString &id) {
    selected_ = id;
    permissions_.clear();
    permissionsMessage_.clear();
    emit changed();
    permissionsRefresh();
}
void IgniteHub::permissionsRefresh() {
    if (!client_->connected() || selected_.isEmpty())
        return;
    auto selected = selected_;
    const auto generation = ++permissionRequest_;
    flint::Writer w;
    w.text(selected.toStdString());
    client_->request(flint::Method::permissions_get, w.data,
        [this, selected, generation](auto error, auto bytes) {
            if (generation != permissionRequest_ || selected != selected_)
                return;
            if (error != flint::Error::ok) {
                permissionsMessage_ = platformError(error);
                emit changed();
                return;
            }
            flint::Reader r{bytes};
            auto count = r.n(2);
            if (count > 64)
                return;
            QVariantList result;
            for (unsigned i = 0; i < count; ++i) {
                QVariantMap p;
                p["name"] = QString::fromStdString(r.text(80));
                p["scope"] = QString::fromStdString(r.text(192));
                p["access"] = int(r.n(1));
                p["enabled"] = bool(r.n(1));
                result.append(p);
            }
            if (r.done()) {
                permissions_ = result;
                emit changed();
            }
        });
}
void IgniteHub::grant(const QString &name, const QString &scope, bool enabled) {
    if (!client_->connected() || selected_.isEmpty())
        return;
    const auto generation = permissionRequest_;
    flint::Writer w;
    w.text(selected_.toStdString());
    w.text(name.toStdString());
    w.text(scope.toStdString());
    w.n(enabled, 1);
    client_->request(flint::Method::permission_set, w.data, [this, generation](auto e, auto) {
        if (generation != permissionRequest_)
            return;
        permissionsMessage_ = platformError(e);
        if (e == flint::Error::ok)
            permissionsRefresh();
        emit changed();
    });
}
void IgniteHub::install(const QUrl &url) {
    if (busy_ || !client_->connected())
        return;
    QFile file(url.toLocalFile());
    if (!url.isLocalFile() || !file.open(QIODevice::ReadOnly) || file.size() < 64 ||
        file.size() > flint::MaxPackageBytes) {
        message_ = "Выберите бинарный Ignite до 1 МиБ";
        emit changed();
        return;
    }
    flint::File input;
    flint::Package package;
    flint::Manifest manifest;
    if (!input.open(file.fileName().toStdString()))
        return;
    auto error = package.open(input.source());
    if (error == flint::Error::ok)
        error = flint::validateApplication(
            package, manifest, {48 * 1024, 0, 4096}, client_->capabilities());
    if (error != flint::Error::ok) {
        message_ = platformError(error);
        emit changed();
        return;
    }
    install_ = file.readAll();
    busy_ = true;
    progress_ = 0;
    message_ = "Передача пакета…";
    emit changed();
    flint::Writer w;
    w.n(install_.size());
    w.text(QCryptographicHash::hash(install_, QCryptographicHash::Sha256).toHex().toStdString());
    client_->request(flint::Method::install_begin, w.data, [this](auto e, auto bytes) {
        if (e != flint::Error::ok) {
            result(e);
            return;
        }
        flint::Reader r{bytes};
        offset_ = r.n();
        if (!r.done() || offset_ > uint32_t(install_.size())) {
            result(flint::Error::invalid_argument);
            return;
        }
        chunk();
    });
}
void IgniteHub::chunk() {
    if (!busy_)
        return;
    if (offset_ == uint32_t(install_.size())) {
        progress_ = 0.95;
        message_ = tr("Verifying and activating package…");
        emit changed();
        client_->request(flint::Method::install_commit, {}, [this](auto e, auto) {
            progress_ = e == flint::Error::ok ? 1 : progress_;
            install_.clear();
            result(e);
        });
        return;
    }
    flint::Writer w;
    w.n(offset_);
    auto size = std::min<uint32_t>(1024, install_.size() - offset_);
    w.raw({reinterpret_cast<const uint8_t *>(install_.constData() + offset_), size});
    client_->request(flint::Method::install_chunk, w.data, [this, size](auto e, auto bytes) {
        if (e != flint::Error::ok) {
            result(e);
            return;
        }
        flint::Reader r{bytes};
        auto offset = r.n();
        if (!r.done() || offset != offset_ + size) {
            result(flint::Error::invalid_argument);
            return;
        }
        offset_ = offset;
        progress_ = 0.95 * double(offset_) / install_.size();
        emit changed();
        chunk();
    });
}
void IgniteHub::launch(const QString &id, const QString &component) {
    flint::Writer w;
    w.text(id.toStdString());
    w.text(component.toStdString());
    client_->request(flint::Method::launch, w.data, [this](auto e, auto) { result(e); });
}
void IgniteHub::remove(const QString &id) {
    flint::Writer w;
    w.text(id.toStdString());
    client_->request(flint::Method::remove, w.data, [this](auto e, auto) { result(e); });
}
void IgniteHub::rollback(const QString &id) {
    flint::Writer w;
    w.text(id.toStdString());
    client_->request(flint::Method::rollback, w.data, [this](auto e, auto) { result(e); });
}
void IgniteHub::setRole(int role, const QString &id, const QString &component) {
    flint::Writer w;
    w.n(role, 1);
    w.text((id + "/" + component).toStdString());
    client_->request(flint::Method::role_set, w.data, [this](auto e, auto) { result(e); });
}
