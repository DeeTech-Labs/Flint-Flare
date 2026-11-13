#include "Sensors.h"
#include <QPointer>
#include <algorithm>
#include <bit>
#include <cmath>

Sensors::Sensors(PlatformClient *client) : client_(client) {
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &Sensors::poll);
    connect(client_, &PlatformClient::disconnected, this, [this] {
        ++generation_;
        ++settingsGeneration_;
        settingsBusy_ = false;
        settings_.clear();
        settingsMessage_.clear();
        timer_.stop();
        enabled_ = busy_ = leaseRequested_ = false;
        sources_.clear();
        reading_.clear();
        message_ = tr("Disconnected");
        emit changed();
    });
    connect(client_, &PlatformClient::ready, this, [this] {
        if (panelActive_) {
            refresh();
            refreshSettings();
        }
    });
}
void Sensors::request(
    flint::Method method, flint::Bytes payload, PlatformClient::Callback callback) {
    const auto generation = generation_;
    client_->request(method, std::move(payload),
        [guard = QPointer<Sensors>(this), generation, callback = std::move(callback)](
            auto e, auto data) {
            if (guard && generation == guard->generation_)
                callback(e, std::move(data));
        });
}
void Sensors::setPanelActive(bool active) {
    if (active == panelActive_)
        return;
    panelActive_ = active;
    if (!active)
        stop();
    else {
        refresh();
        refreshSettings();
    }
    emit changed();
}
QString Sensors::stateLabel(int state) const {
    switch (state) {
    case 0:
        return tr("Absent");
    case 1:
        return tr("Pending");
    case 2:
        return tr("Valid");
    case 3:
        return tr("Stale");
    case 4:
        return tr("Failed");
    case 5:
        return tr("Initializing");
    default:
        return tr("Unavailable");
    }
}
QString Sensors::channelLabel(int channel) const {
    switch (channel) {
    case 0:
        return tr("Temperature (°C)");
    case 1:
        return tr("Pressure (Pa)");
    case 2:
        return tr("Humidity (%)");
    case 3:
        return tr("Ambient light (lux)");
    case 4:
        return tr("Acceleration (m/s²)");
    case 5:
        return tr("Angular velocity (rad/s)");
    default:
        return tr("Unknown channel");
    }
}
void Sensors::refresh() {
    if (!panelActive_ || !client_->connected() || busy_ || enabled_)
        return;
    busy_ = true;
    request(flint::Method::sensor_catalog, {}, [this](auto error, auto data) {
        busy_ = false;
        if (error != flint::Error::ok) {
            sources_.clear();
            message_ = platformError(error);
            emit changed();
            return;
        }
        flint::Reader r{data};
        const auto count = r.n(1);
        QVariantList result;
        bool valid = count <= flint::sensors::MaxSources;
        for (unsigned i = 0; valid && i < count; ++i) {
            QVariantMap s;
            const auto id = r.n(2), channels = r.n(1);
            const auto min = r.n(), def = r.n(), max = r.n(), stale = r.n();
            const auto priority = r.n(1), simulation = r.n(1), temperature = r.n(1),
                       coordinates = r.n(1);
            const auto state = r.n(1), reason = r.n(2);
            s = {{"id", int(id)}, {"channels", int(channels)}, {"min", qulonglong(min)},
                {"default", qulonglong(def)}, {"max", qulonglong(max)},
                {"staleAfter", qulonglong(stale)}, {"priority", int(priority)},
                {"simulation", bool(simulation)}, {"temperature", int(temperature)},
                {"coordinates", int(coordinates)}, {"state", int(state)}, {"reason", int(reason)},
                {"generation", qulonglong(r.n(8))}, {"sampleId", qulonglong(r.n(8))},
                {"at", qulonglong(r.n(8))}, {"age", qulonglong(r.n(8))}};
            valid = r.valid && id && channels && !(channels & ~flint::sensors::AllChannels) &&
                min && stale && min <= def && def <= max && simulation <= 1 && temperature <= 2 &&
                coordinates <= 1 && state <= 5;
            for (const auto &old : result)
                valid = valid && old.toMap().value("id").toInt() != int(id);
            result.append(s);
        }
        if (!valid || !r.done()) {
            sources_.clear();
            message_ = tr("Invalid sensor response");
        } else {
            sources_ = result;
            message_ = sources_.empty() ? tr("No sensor sources")
                                        : tr("Catalog loaded; diagnostics are stopped");
        }
        emit changed();
    });
    emit changed();
}
void Sensors::start(int source) {
    if (!panelActive_ || !client_->connected() || busy_ || enabled_)
        return;
    QVariantMap descriptor;
    for (const auto &entry : sources_)
        if (entry.toMap().value("id").toInt() == source)
            descriptor = entry.toMap();
    if (descriptor.empty())
        return;
    const auto minimum = descriptor.value("min").toUInt(),
               maximum = descriptor.value("max").toUInt();
    if (maximum < 200) {
        message_ = tr("Source cannot meet diagnostic rate limits");
        emit changed();
        return;
    }
    staleAfter_ = descriptor.value("staleAfter").toUInt();
    source_ = uint16_t(source);
    channels_ = uint8_t(descriptor.value("channels").toUInt());
    interval_ = std::clamp<uint32_t>(500, std::max<uint32_t>(200, minimum), maximum);
    ++generation_;
    lastRead_.invalidate();
    lease(false);
}
void Sensors::lease(bool renew) {
    busy_ = leaseRequested_ = true;
    flint::Writer w;
    w.n(1, 1);
    w.n(source_, 2);
    w.n(channels_, 1);
    w.n(interval_);
    w.n(30000);
    request(flint::Method::sensor_diagnostic, w.data, [this, renew](auto error, auto data) {
        busy_ = false;
        if (error != flint::Error::ok || !data.empty()) {
            stop();
            message_ =
                error == flint::Error::ok ? tr("Invalid sensor response") : platformError(error);
        } else {
            enabled_ = true;
            renewal_.restart();
            timer_.start(200);
            message_ = tr("Diagnostic lease active");
            if (!renew)
                poll();
        }
        emit changed();
    });
    emit changed();
}
void Sensors::stop() {
    const bool hadDemand = leaseRequested_;
    leaseRequested_ = false;
    ++generation_;
    timer_.stop();
    enabled_ = busy_ = false;
    reading_.clear();
    message_ = tr("Diagnostics stopped");
    if (hadDemand && client_->connected()) {
        flint::Writer w;
        w.n(0, 1);
        request(flint::Method::sensor_diagnostic, w.data, [this](auto error, auto data) {
            if (error != flint::Error::ok)
                message_ = platformError(error);
            else if (!data.empty())
                message_ = tr("Invalid sensor response");
            emit changed();
        });
    }
    emit changed();
}
void Sensors::poll() {
    if (reading_.value("status").toInt() == 2 && sampleReceived_.isValid()) {
        const auto elapsed = uint64_t(sampleReceived_.elapsed());
        const auto age = reading_.value("age").toULongLong();
        if (age >= staleAfter_ || elapsed >= staleAfter_ - age) {
            reading_.insert("status", 3);
            reading_.insert(
                "age", qulonglong(age > UINT64_MAX - elapsed ? UINT64_MAX : age + elapsed));
            reading_.insert("channels", 0);
            reading_.insert("values", QVariantList{});
            emit changed();
        }
    }
    if (!panelActive_ || !enabled_ || busy_ || !client_->connected())
        return;
    if (renewal_.elapsed() >= 10000) {
        lease(true);
        return;
    }
    if (lastRead_.isValid() && lastRead_.elapsed() < interval_)
        return;
    lastRead_.restart();
    busy_ = true;
    flint::Writer w;
    w.n(2, 1);
    request(flint::Method::sensor_diagnostic, w.data, [this](auto error, auto data) {
        busy_ = false;
        lastRead_.restart();
        if (error != flint::Error::ok) {
            stop();
            message_ = platformError(error);
            emit changed();
            return;
        }
        flint::Reader r{data};
        const auto source = r.n(2), channels = r.n(1);
        QVariantMap sample{{"source", int(source)}, {"channels", int(channels)},
            {"generation", qulonglong(r.n(8))}, {"sampleId", qulonglong(r.n(8))},
            {"at", qulonglong(r.n(8))}, {"age", qulonglong(r.n(8))}};
        const auto status = r.n(1), reason = r.n(2), drops = r.n(), simulation = r.n(1);
        sample.insert("status", int(status));
        sample.insert("reason", int(reason));
        sample.insert("drops", qulonglong(drops));
        sample.insert("simulation", bool(simulation));
        QVariantList values;
        bool valid = source == source_ && (status == 2 ? channels == channels_ : channels == 0) &&
            status <= 5 && simulation <= 1;
        for (int c = 0; c < 6; ++c)
            if (channels & (1u << c)) {
                const double x = std::bit_cast<double>(r.n(8)), y = std::bit_cast<double>(r.n(8)),
                             z = std::bit_cast<double>(r.n(8));
                valid = valid && std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
                values.append(QVariantMap{{"channel", c}, {"x", x}, {"y", y}, {"z", z}});
            }
        if (!valid || !r.done()) {
            stop();
            message_ = tr("Invalid sensor response");
        } else {
            sample.insert("values", values);
            reading_ = sample;
            sampleReceived_.restart();
        }
        emit changed();
    });
}

void Sensors::settingsRequest(
    flint::Method method, flint::Bytes payload, PlatformClient::Callback callback) {
    const auto generation = settingsGeneration_;
    client_->request(method, std::move(payload),
        [guard = QPointer<Sensors>(this), generation, callback = std::move(callback)](
            auto error, auto data) {
            if (guard && generation == guard->settingsGeneration_)
                callback(error, std::move(data));
        });
}
void Sensors::refreshSettings() {
    if (!panelActive_ || !client_->connected() || settingsBusy_)
        return;
    settingsBusy_ = true;
    readSettings(false);
    emit changed();
}
void Sensors::readSettings(bool afterSave, const QString &operationError) {
    settingsRequest(
        flint::Method::settings_get, {}, [this, afterSave, operationError](auto error, auto data) {
            settingsBusy_ = false;
            if (error != flint::Error::ok) {
                settings_.clear();
                settingsMessage_ = operationError.isEmpty()
                    ? platformError(error)
                    : operationError + "; " + platformError(error);
                emit changed();
                return;
            }
            flint::Reader r{data};
            const auto count = r.n(2);
            QVariantMap values;
            bool valid = count <= 128;
            for (unsigned i = 0; valid && i < count; ++i) {
                const auto key = QString::fromStdString(r.text(96)),
                           value = QString::fromStdString(r.text(192));
                valid = r.valid && !values.contains(key);
                values.insert(key, value);
            }
            auto flag = [&](const char *key) {
                return !values.contains(key) || values.value(key) == "0" ||
                    values.value(key) == "1";
            };
            valid = valid && flag("brightness_supported") && flag("mute_supported") &&
                flag("audio_muted");
            for (const auto &key : {"brightness_manual", "brightness_fallback"})
                if (values.contains(key)) {
                    bool ok = false;
                    const auto level = values.value(key).toString().toUInt(&ok);
                    valid = valid && ok && level >= 5 && level <= 100;
                }
            if (values.contains("brightness_mode"))
                valid = valid &&
                    (values.value("brightness_mode") == "manual" ||
                        values.value("brightness_mode") == "automatic");
            if (values.value("brightness_supported") == "1")
                valid = valid && values.contains("brightness_mode") &&
                    values.contains("brightness_manual") && values.contains("brightness_fallback");
            if (values.value("mute_supported") == "1")
                valid = valid && values.contains("audio_muted");
            if (!valid || !r.done()) {
                settings_.clear();
                settingsMessage_ = operationError.isEmpty()
                    ? tr("Invalid settings response")
                    : operationError + "; " + tr("Invalid settings response");
            } else {
                values.insert("brightness_supported", values.value("brightness_supported") == "1");
                values.insert("mute_supported", values.value("mute_supported") == "1");
                settings_ = values;
                if (!operationError.isEmpty())
                    settingsMessage_ = operationError;
                else if (afterSave)
                    settingsMessage_ =
                        values.value(pendingSettingKey_).toString() == pendingSettingValue_
                        ? tr("Settings confirmed by device")
                        : tr("Device did not confirm requested setting");
                else
                    settingsMessage_.clear();
            }
            emit changed();
        });
}
void Sensors::setSetting(const QString &key, const QString &value) {
    if (!panelActive_ || !client_->connected() || settingsBusy_)
        return;
    bool valid = false;
    if (key == "audio_muted")
        valid = settings_.value("mute_supported").toBool() && (value == "0" || value == "1");
    else if (settings_.value("brightness_supported").toBool()) {
        if (key == "brightness_mode") {
            bool light = false;
            for (const auto &source : sources_)
                light = light ||
                    (source.toMap().value("channels").toInt() &
                        flint::sensors::bit(flint::sensors::Channel::ambient_light));
            valid = value == "manual" || (value == "automatic" && light);
        } else if (key == "brightness_manual" || key == "brightness_fallback") {
            bool ok = false;
            const auto level = value.toUInt(&ok);
            valid = ok && level >= 5 && level <= 100;
        }
    }
    if (!valid) {
        settingsMessage_ = tr("Setting is unavailable or invalid");
        emit changed();
        return;
    }
    settingsBusy_ = true;
    settingsMessage_ = tr("Applying settings…");
    pendingSettingKey_ = key;
    pendingSettingValue_ = value;
    flint::Writer w;
    w.text(key.toStdString());
    w.text(value.toStdString());
    settingsRequest(flint::Method::settings_set, w.data, [this](auto error, auto data) {
        if (error != flint::Error::ok || !data.empty()) {
            // Policy persistence may succeed before a hardware adapter reports failure.
            // Read back the durable policy, retaining the operation failure in the UI.
            readSettings(false,
                error == flint::Error::ok ? tr("Invalid settings response") : platformError(error));
        } else
            readSettings(true);
    });
    emit changed();
}
