#include "BackgroundServices.h"
#include <QPointer>
#include <algorithm>

BackgroundServices::BackgroundServices(PlatformClient *client) : client_(client) {
    timer_.setInterval(500);
    connect(&timer_, &QTimer::timeout, this, &BackgroundServices::refresh);
    connect(client_, &PlatformClient::ready, this, [this] {
        notice_ = queryNotice_ = Notice::none;
        if (active_) {
            timer_.start();
            refresh();
        }
        emit changed();
    });
    connect(client_, &PlatformClient::disconnected, this, [this] {
        ++connectionGeneration_;
        ++fetchGeneration_;
        timer_.stop();
        fetching_ = mutating_ = false;
        replaceRows({});
        incoming_.clear();
        policy_.clear();
        notice_ = Notice::disconnected;
        queryNotice_ = Notice::none;
        emit changed();
    });
}
void BackgroundServices::setPanelActive(bool active) {
    if (active_ == active)
        return;
    active_ = active;
    if (active && client_->connected()) {
        timer_.start();
        refresh();
    } else {
        timer_.stop();
        ++fetchGeneration_;
        fetching_ = false;
        incoming_.clear();
    }
    // Monitoring never owns a service lifetime. Hiding this panel sends no stop command.
    emit changed();
}
QString BackgroundServices::stateLabel(int state) const {
    switch (state) {
    case 0:
        return tr("Stopped");
    case 1:
        return tr("Starting");
    case 2:
        return tr("Running");
    case 3:
        return tr("Paused");
    case 4:
        return tr("Faulted");
    default:
        return tr("Unknown");
    }
}
QString BackgroundServices::reasonLabel(int reason) const {
    switch (reason) {
    case 0:
        return tr("None");
    case 1:
        return tr("User action");
    case 2:
        return tr("Permission");
    case 3:
        return tr("Low energy");
    case 4:
        return tr("Resources");
    case 5:
        return tr("Update");
    case 6:
        return tr("Error");
    case 7:
        return tr("Quota");
    case 8:
        return tr("Shutdown");
    default:
        return tr("Unknown");
    }
}
bool BackgroundServices::known(const QString &package, const QString &component) const {
    for (const auto &entry : services_) {
        const auto row = entry.toMap();
        if (row.value("package") == package &&
            (component.isEmpty() || row.value("component") == component))
            return true;
    }
    return false;
}
void BackgroundServices::refresh() {
    if (!active_ || !client_->connected() || busy())
        return;
    if (!(client_->capabilities() & flint::CapBackground)) {
        timer_.stop();
        replaceRows({});
        policy_.clear();
        queryNotice_ = Notice::unavailable;
        emit changed();
        return;
    }
    fetching_ = true;
    incoming_.clear();
    incomingPolicy_.clear();
    page(0, ++fetchGeneration_);
    emit changed();
}
void BackgroundServices::page(uint16_t offset, uint64_t generation) {
    flint::Writer w;
    w.n(offset, 2);
    w.n(8, 1);
    client_->request(flint::Method::services, w.data,
        [guard = QPointer<BackgroundServices>(this), generation, offset](auto error, auto data) {
            if (!guard || generation != guard->fetchGeneration_ || !guard->active_)
                return;
            auto &self = *guard;
            if (error != flint::Error::ok) {
                self.fetching_ = false;
                self.replaceRows({});
                self.policy_.clear();
                self.queryError_ = error;
                self.queryNotice_ = Notice::failure;
                emit self.changed();
                return;
            }
            flint::Reader r{data};
            const auto count = r.n(1), more = r.n(1), maximum = r.n(1);
            QVariantMap policy{{"maxServices", int(maximum)}, {"memoryBytes", qulonglong(r.n())},
                {"instructionsPerWindow", qulonglong(r.n())},
                {"nativeCallsPerWindow", qulonglong(r.n())}, {"windowMs", qulonglong(r.n())}};
            const auto lowEnergy = r.n(1);
            policy.insert("lowEnergy", bool(lowEnergy));
            bool valid = count <= 8 && more <= 1 && lowEnergy <= 1 &&
                maximum <= flint::MaxBackgroundServices && (!more || count) &&
                (offset == 0 || policy == self.incomingPolicy_);
            if (!offset)
                self.incomingPolicy_ = policy;
            for (unsigned i = 0; valid && i < count; ++i) {
                const auto packageBytes = r.text(64), componentBytes = r.text(64),
                           labelBytes = r.text(64);
                const auto package = QString::fromStdString(packageBytes),
                           component = QString::fromStdString(componentBytes),
                           label = QString::fromStdString(labelBytes);
                const auto state = r.n(1), reason = r.n(1), failure = r.n(2);
                QVariantMap row{{"package", package}, {"component", component}, {"label", label},
                    {"state", int(state)}, {"reason", int(reason)}, {"error", int(failure)},
                    {"instanceId", qulonglong(r.n(8))}, {"generation", qulonglong(r.n(8))},
                    {"used", qulonglong(r.n())}, {"peak", qulonglong(r.n())},
                    {"reserved", qulonglong(r.n())}, {"callbacks", qulonglong(r.n(8))},
                    {"instructions", qulonglong(r.n(8))}, {"nativeCalls", qulonglong(r.n(8))},
                    {"maxCallbackUs", qulonglong(r.n(8))}, {"wakeups", qulonglong(r.n(8))},
                    {"eventRejected", qulonglong(r.n(8))}, {"eventCoalesced", qulonglong(r.n(8))}};
                const auto execute = r.n(1), autostartGranted = r.n(1), autostart = r.n(1);
                row.insert("executeGranted", bool(execute));
                row.insert("autostartGranted", bool(autostartGranted));
                row.insert("autostart", bool(autostart));
                valid = r.valid && flint::appText(labelBytes) &&
                    !std::any_of(labelBytes.begin(), labelBytes.end(),
                        [](unsigned char c) { return c < 32 || c == 127; }) &&
                    flint::identifier(package.toStdString()) &&
                    flint::identifier(component.toStdString()) && state <= 4 && reason <= 8 &&
                    execute <= 1 && autostartGranted <= 1 && autostart <= 1;
                for (const auto &existing : self.incoming_) {
                    const auto old = existing.toMap();
                    valid = valid &&
                        !(old.value("package") == package && old.value("component") == component);
                }
                self.incoming_.append(row);
            }
            if (!valid || !r.done() || self.incoming_.size() > 4096 ||
                (more && unsigned(offset) + count > 65535)) {
                self.fetching_ = false;
                self.replaceRows({});
                self.policy_.clear();
                self.queryNotice_ = Notice::invalidResponse;
                emit self.changed();
                return;
            }
            if (more) {
                self.page(uint16_t(offset + count), generation);
                return;
            }
            self.policy_ = policy;
            self.queryNotice_ = Notice::none;
            self.fetching_ = false;
            self.replaceRows(self.incoming_);
            emit self.changed();
        });
}
void BackgroundServices::mutate(flint::Method method, flint::Bytes payload) {
    ++fetchGeneration_;
    fetching_ = false;
    incoming_.clear();
    mutating_ = true;
    notice_ = Notice::applying;
    const auto generation = connectionGeneration_;
    client_->request(method, std::move(payload),
        [guard = QPointer<BackgroundServices>(this), generation](auto error, auto data) {
            if (!guard || generation != guard->connectionGeneration_)
                return;
            auto &self = *guard;
            self.mutating_ = false;
            self.error_ = error;
            self.notice_ = error != flint::Error::ok ? Notice::failure
                : data.empty()                       ? Notice::confirmed
                                                     : Notice::invalidResponse;
            // An error can still follow a durable policy update. Refresh metadata without erasing
            // it.
            if (self.active_)
                self.refresh();
            emit self.changed();
        });
    emit changed();
}
void BackgroundServices::control(int action, const QString &package, const QString &component) {
    if (!active_ || !client_->connected() || mutating_ || action < 0 || action > 6 ||
        !known(package, component))
        return;
    flint::Writer w;
    w.n(action, 1);
    w.text(package.toStdString());
    w.text(component.toStdString());
    mutate(flint::Method::service_control, std::move(w.data));
}
void BackgroundServices::grant(const QString &package, const QString &permission, bool enabled) {
    if (!active_ || !client_->connected() || mutating_ || !known(package) ||
        (permission != "app.background.execute" && permission != "app.background.autostart"))
        return;
    flint::Writer w;
    w.text(package.toStdString());
    w.text(permission.toStdString());
    w.text("");
    w.n(enabled, 1);
    mutate(flint::Method::permission_set, std::move(w.data));
}

int BackgroundServices::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : services_.size();
}
QVariant BackgroundServices::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= services_.size() ||
        role != Qt::UserRole + 1)
        return {};
    return services_[index.row()];
}
QHash<int, QByteArray> BackgroundServices::roleNames() const {
    return {{Qt::UserRole + 1, "record"}};
}
void BackgroundServices::replaceRows(QVariantList rows) {
    bool same = rows.size() == services_.size();
    for (qsizetype i = 0; same && i < rows.size(); ++i) {
        const auto before = services_[i].toMap(), after = rows[i].toMap();
        same = before.value("package") == after.value("package") &&
            before.value("component") == after.value("component");
    }
    if (!same) {
        beginResetModel();
        services_ = std::move(rows);
        endResetModel();
        return;
    }
    for (qsizetype i = 0; i < rows.size(); ++i)
        if (services_[i] != rows[i]) {
            services_[i] = rows[i];
            const auto item = index(int(i));
            emit dataChanged(item, item, {Qt::UserRole + 1});
        }
}

QString BackgroundServices::message() const {
    const auto text = [this](Notice notice, flint::Error error) -> QString {
        switch (notice) {
        case Notice::none:
            return {};
        case Notice::disconnected:
            return tr("Disconnected");
        case Notice::unavailable:
            return tr("Background services unavailable");
        case Notice::invalidResponse:
            return tr("Invalid background service response");
        case Notice::applying:
            return tr("Applying service action…");
        case Notice::confirmed:
            return tr("Service action confirmed");
        case Notice::failure:
            return platformError(error);
        }
        return {};
    };
    const auto operation = text(notice_, error_), query = text(queryNotice_, queryError_);
    return operation.isEmpty() ? query : query.isEmpty() ? operation : operation + "\n" + query;
}
