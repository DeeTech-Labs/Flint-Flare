#include "PlatformClient.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStandardPaths>
#include <chrono>
#include <cmath>
#include <limits>
static uint64_t monotonic() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
QString platformError(flint::Error error) {
    static const char *const names[] = {"Done", "Invalid request", "Invalid package",
        "Incompatible build", "Not supported", "Permission denied", "User consent required",
        "Source unavailable", "Cancelled", "Quota exceeded", "Resources exhausted", "Storage error",
        "Integrity failure", "Invalid state", "Stale handle", "Execution limit exceeded",
        "State conflict"};
    static_assert(std::size(names) == unsigned(flint::Error::conflict) + 1);
    const auto index = unsigned(error);
    return QCoreApplication::translate(
        "PlatformClient", index < std::size(names) ? names[index] : "Protocol error");
}

PlatformClient::PlatformClient(QObject *parent) : QObject(parent) {
    timeSyncTimeout_.setSingleShot(true);
    timeSyncTimeout_.setInterval(5000);
    connect(&timeSyncTimeout_, &QTimer::timeout, this, [this] {
        if (timeSyncBusy())
            completeTimeSync(false, tr("Time synchronization confirmation timed out"));
    });
    simulationTick_.setSingleShot(true);
    simulationTick_.setTimerType(Qt::PreciseTimer);
    connect(&simulationTick_, &QTimer::timeout, this, [this] {
        if (core_ && core_->tick())
            emit simulationSceneChanged();
        drainSimulationEvents();
        scheduleSimulation();
    });
    pairingPoll_.setInterval(1000);
    connect(&pairingPoll_, &QTimer::timeout, this, [this] {
        if (serial_.isOpen())
            pairing("status");
    });
    installTimeout_.setSingleShot(true);
    installTimeout_.setInterval(45000);
    connect(&installTimeout_, &QTimer::timeout, this, [this] {
        ++timeouts_;
        fail(tr("Package verification timed out"));
    });
    closeTimeout_.setSingleShot(true);
    closeTimeout_.setInterval(100);
    connect(&closeTimeout_, &QTimer::timeout, this, [this] {
        if (!closing_)
            return;
        ++closeTimeouts_;
        finishDisconnect();
    });
    timeout_.setSingleShot(true);
    timeout_.setInterval(45000);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        ++timeouts_;
        fail("Устройство не ответило вовремя");
    });
    connect(&serial_, &QSerialPort::readyRead, this, [this] { receive(serial_.readAll(), true); });
    connect(&serial_, &QSerialPort::errorOccurred, this, [this](auto error) {
        if (error == QSerialPort::ResourceError) {
            if (closing_)
                finishDisconnect();
            else
                fail(serial_.errorString());
        }
    });
}
QVariantList PlatformClient::ports() const {
    QVariantList result;
    for (auto &p : QSerialPortInfo::availablePorts())
        result.append(QVariantMap{
            {"path", p.systemLocation()}, {"label", p.portName() + " · " + p.description()}});
    return result;
}
QVariantList PlatformClient::bleDevices() const {
    return ble_ ? ble_->devices() : QVariantList{};
}
QString PlatformClient::pairingCode() const {
    return ble_ ? ble_->pairingCode() : QString{};
}
void PlatformClient::ensureBle() {
    if (ble_)
        return;
    ble_ = std::make_unique<BleTransport>();
    connect(ble_.get(), &BleTransport::devicesChanged, this, &PlatformClient::changed);
    connect(ble_.get(), &BleTransport::pairingCodeChanged, this, &PlatformClient::changed);
    connect(ble_.get(), &BleTransport::received, this,
        [this](const QByteArray &bytes) { receive(bytes); });
    connect(ble_.get(), &BleTransport::error, this, &PlatformClient::fail);
    connect(ble_.get(), &BleTransport::writable, this, &PlatformClient::sendNext);
    connect(ble_.get(), &BleTransport::readyChanged, this, [this] {
        if (ble_->ready())
            handshake();
    });
}
void PlatformClient::disconnectDevice() {
    resetConnection();
}
void PlatformClient::resetConnection(std::function<void()> afterClose) {
    afterClose_ = std::move(afterClose);
    if (closing_)
        return;
    const bool graceful = serial_.isOpen() && (capabilities_ & flint::CapSessionClose);
    closing_ = true;
    ++generation_;
    simulationTick_.stop();
    pairingPoll_.stop();
    watchPairingCode_.clear();
    timeout_.stop();
    installTimeout_.stop();
    connected_ = false;
    resetTimeSync();
    blePolicyKnown_ = blePolicyBusy_ = false;
    capabilities_ = 0;
    if (core_)
        core_->disconnect(session_);
    queue_.clear();
    pending_.reset();
    installPending_.reset();
    if (ble_)
        ble_->close();
    parser_.reset();
    usbParser_.reset();
    info_.clear();
    status_ = graceful ? tr("Disconnecting…") : QString::fromUtf8("Отключено");
    emit disconnected();
    emit changed();
    if (!graceful) {
        finishDisconnect();
        return;
    }
    if (++nextId_ == 0)
        ++nextId_;
    closeId_ = nextId_;
    const auto bytes = flint::encodeFrame({flint::Method::session_close, closeId_, false, {}});
    if (serial_.write(reinterpret_cast<const char *>(bytes.data()), bytes.size()) !=
        qint64(bytes.size())) {
        finishDisconnect();
        return;
    }
    closeTimeout_.start();
}
void PlatformClient::finishDisconnect() {
    if (finishingDisconnect_)
        return;
    finishingDisconnect_ = true;
    closeTimeout_.stop();
    serial_.close();
    if (ble_)
        ble_->close();
    core_.reset();
    simulation_.reset();
    parser_.reset();
    usbParser_.reset();
    status_ = "Отключено";
    closing_ = finishingDisconnect_ = false;
    auto continuation = std::move(afterClose_);
    afterClose_ = {};
    const auto generation = generation_;
    emit changed();
    if (continuation && generation == generation_)
        continuation();
}
void PlatformClient::fail(const QString &message) {
    if (finishingDisconnect_)
        return;
    afterClose_ = {};
    closing_ = true;
    ++generation_;
    simulationTick_.stop();
    pairingPoll_.stop();
    watchPairingCode_.clear();
    timeout_.stop();
    installTimeout_.stop();
    connected_ = false;
    resetTimeSync();
    blePolicyKnown_ = blePolicyBusy_ = false;
    capabilities_ = 0;
    queue_.clear();
    pending_.reset();
    installPending_.reset();
    info_.clear();
    if (core_)
        core_->disconnect(session_);
    finishDisconnect();
    status_ = message;
    emit disconnected();
    emit changed();
    emit failure(message);
}
void PlatformClient::connectUsb(const QString &port) {
    resetConnection([this, port] { openUsb(port); });
}
void PlatformClient::openUsb(const QString &port) {
    serial_.setPortName(port);
    serial_.setBaudRate(115200);
    if (!serial_.open(QIODevice::ReadWrite)) {
        fail(serial_.errorString());
        return;
    }
    handshake();
}
void PlatformClient::scanBle() {
    ensureBle();
    ble_->scan();
    emit changed();
}
void PlatformClient::connectBle(const QString &address) {
    if (closing_) {
        afterClose_ = [this, address] { connectBle(address); };
        return;
    }
    if (serial_.isOpen()) {
        ++generation_;
        // Retain the authenticated local USB channel for numeric comparison.
        timeout_.stop();
        installTimeout_.stop();
        connected_ = false;
        blePolicyKnown_ = blePolicyBusy_ = false;
        queue_.clear();
        pending_.reset();
        installPending_.reset();
        parser_.reset();
        usbParser_.reset();
        info_.clear();
        emit disconnected();
    } else {
        resetConnection([this, address] { openBle(address); });
        return;
    }
    openBle(address);
}
void PlatformClient::openBle(const QString &address) {
    ensureBle();
    ble_->open(address);
    status_ = "Подключение BLE…";
    emit changed();
}
void PlatformClient::confirmPairing(bool accepted) {
    if (ble_)
        ble_->confirmPairing(accepted);
}
void PlatformClient::pairing(const QString &action) {
    if (!serial_.isOpen()) {
        emit failure(tr("Pairing requires a local USB connection."));
        return;
    }
    flint::Writer writer;
    writer.text(action.toStdString());
    request(flint::Method::pairing, writer.data, [this, action](auto error, auto data) {
        if (error != flint::Error::ok) {
            pairingPoll_.stop();
            emit failure(platformError(error));
            return;
        }
        const auto state =
            QString::fromUtf8(reinterpret_cast<const char *>(data.data()), data.size());
        const auto code = QRegularExpression("code=(\\d+)").match(state);
        watchPairingCode_ = code.hasMatch() ? code.captured(1).rightJustified(6, '0') : QString();
        if (state.startsWith("open=1")) {
            pairingPoll_.start();
        } else {
            pairingPoll_.stop();
        }
        if (action != "status") {
            status_ = state;
        }
        emit changed();
    });
}
void PlatformClient::startSimulator() {
    resetConnection([this] { startSimulatorNow(); });
}
void PlatformClient::startSimulatorNow() {
    simulation_ = std::make_unique<QTemporaryDir>();
    auto root = simulation_->path();
    QDir().mkpath(root + "/packages");
    QDir().mkpath(root + "/factory");
    // Factory binaries are built by the same compiler as the firmware image.
    for (auto &name : QDir(":/factory").entryList({"*.ignit"}, QDir::Files))
        QFile::copy(":/factory/" + name, root + "/factory/" + name);
    sensorSimulation_ = std::make_unique<flint::sensors::Simulation>();
    virtualValues_ = {};
    virtualValues_[0][0].x = 22.0;
    virtualValues_[0][1].x = 101325.0;
    virtualValues_[0][2].x = 45.0;
    virtualValues_[1][3].x = 150.0;
    virtualValues_[2][4].z = 9.80665;
    virtualValues_[3][0].x = 36.0;
    using namespace flint::sensors;
    sensorSimulation_->add(
        {1, Mask(bit(Channel::temperature) | bit(Channel::pressure) | bit(Channel::humidity)), 1000,
            1000, 60000, 5000, 0, true, TemperatureKind::ambient},
        virtualValues_[0]);
    sensorSimulation_->add(
        {2, bit(Channel::ambient_light), 200, 1000, 60000, 5000, 0, true}, virtualValues_[1]);
    sensorSimulation_->add(
        {3, Mask(bit(Channel::acceleration) | bit(Channel::angular_velocity)), 100, 200, 60000,
            2000, 0, true, TemperatureKind::unspecified, Coordinates::device},
        virtualValues_[2]);
    sensorSimulation_->add(
        {4, bit(Channel::temperature), 5000, 5000, 60000, 15000, 10, true, TemperatureKind::die},
        virtualValues_[3]);
    simulationOffset_ = 0;
    flint::Platform p;
    p.sensors = sensorSimulation_.get();
    virtualBrightness_ = 0;
    virtualMuted_ = false;
    auto notifyVirtualOutput = [this] {
        QTimer::singleShot(0, this, [this, generation = generation_] {
            if (generation == generation_ && core_)
                emit changed();
        });
    };
    p.brightness_apply = [this, notifyVirtualOutput](unsigned percent) {
        if (percent < 5 || percent > 100)
            return flint::Error::invalid_argument;
        virtualBrightness_ = percent;
        notifyVirtualOutput();
        return flint::Error::ok;
    };
    p.mute_apply = [this, notifyVirtualOutput](bool muted) {
        virtualMuted_ = muted;
        notifyVirtualOutput();
        return flint::Error::ok;
    };
#ifdef FLINT_ENABLE_LUA
    p.capabilities |= flint::CapBackground | flint::CapComponentIpc;
    p.background.maxServices = 2;
    p.background.memoryBytes = 131072;
    p.background_available_memory = [] { return flint::MemoryBudget{512 * 1024, 0, 4096}; };
#endif
    virtualLowEnergy_ = false;
    p.background_low_energy = [this] { return virtualLowEnergy_; };
    p.microseconds = [] {
        return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                .count());
    };
    p.build_id = "desktop-simulator-v1";
    p.memory = {48 * 1024, 0, 4096};
    p.monotonic = [this] { return simulationNow(); };
    p.free_storage = [] { return uint64_t(1024 * 1024); };
    core_ = std::make_unique<flint::Core>(
        (root + "/packages").toStdString(), (root + "/factory").toStdString(), p);
    auto error = core_->begin();
    if (error != flint::Error::ok) {
        fail(platformError(error));
        return;
    }
    session_ = {true, false, 1};
    handshake();
}
void PlatformClient::handshake() {
    status_ = "Проверка совместимости…";
    emit changed();
    flint::Writer w;
    w.n(flint::ProtocolVersion, 2);
    w.n(flint::ContractRevision);
    w.text("flint-flare-" FLINT_FLARE_VERSION);
    w.n(flint::CapPackages | flint::CapDeclarative, 8);
    request(flint::Method::hello, w.data, [this](auto error, auto bytes) {
        if (error != flint::Error::ok) {
            fail(platformError(error));
            return;
        }
        flint::Reader r{bytes};
        auto version = r.n(2), revision = r.n();
        auto build = r.text(96), profile = r.text(96);
        auto caps = r.n(8);
        if (!r.done() || profile != flint::DeviceProfile ||
            flint::compatible({uint16_t(version), uint32_t(revision), build, caps},
                flint::CapPackages | flint::CapDeclarative) != flint::Error::ok) {
            fail("Несовместимый контракт платформы");
            return;
        }
        capabilities_ = caps;
        connected_ = true;
        status_ = core_ ? "Локальный симулятор" : "Подключено";
        emit changed();
        refreshInfo();
        if (caps & flint::CapBlePolicy)
            refreshBlePolicy();
        emit ready();
        if (autoSyncTime())
            syncTime();
    });
}
void PlatformClient::request(flint::Method method, flint::Bytes payload, Callback callback) {
    if (closing_) {
        callback(flint::Error::cancelled, {});
        return;
    }
    if (!core_ && !serial_.isOpen() && !(ble_ && ble_->ready())) {
        callback(flint::Error::source_unavailable, {});
        return;
    }
    if (method == flint::Method::companion_sample) {
        // Samples have no value after a newer sample or demand generation arrives.
        std::erase_if(queue_, [](const auto &entry) {
            return entry.frame.method == flint::Method::companion_sample;
        });
    }
    if (queue_.size() >= 64) {
        ++queueRejected_;
        callback(flint::Error::quota_exceeded, {});
        return;
    }
    if (++nextId_ == 0)
        ++nextId_;
    ++requests_;
    Pending entry{{method, nextId_, false, std::move(payload)},
        [this, generation = generation_, callback = std::move(callback)](auto error, auto data) {
            if (generation == generation_)
                callback(error, std::move(data));
        },
        monotonic()};
    if (method == flint::Method::companion_sample)
        queue_.push_back(std::move(entry));
    else {
        const auto sample = std::find_if(queue_.begin(), queue_.end(), [](const auto &queued) {
            return queued.frame.method == flint::Method::companion_sample;
        });
        queue_.insert(sample, std::move(entry));
    }
    queuePeak_ = std::max<uint64_t>(queuePeak_, queue_.size());
    sendNext();
}
void PlatformClient::sendNext() {
    if (closing_ || pending_ || queue_.empty())
        return;
    const auto method = queue_.front().frame.method;
    const bool usb =
        serial_.isOpen() && (method == flint::Method::pairing || !(ble_ && ble_->ready()));
    if (!core_ && !usb && ble_ && !ble_->canWrite())
        return;
    if (method == flint::Method::install_commit && installPending_) {
        auto rejected = std::move(queue_.front());
        queue_.pop_front();
        ++queueRejected_;
        rejected.callback(flint::Error::invalid_state, {});
        QTimer::singleShot(0, this, [this] { sendNext(); });
        return;
    }
    auto next = std::make_unique<Pending>(std::move(queue_.front()));
    queue_.pop_front();
    next->sentAt = monotonic();
    maxQueueMs_ = std::max(maxQueueMs_, next->sentAt - next->queuedAt);
    const auto wire = flint::encodeFrame(next->frame);
    if (wire.empty()) {
        fail("Запрос превышает лимит протокола");
        return;
    }
    const auto id = next->frame.id;
    if (method == flint::Method::install_commit) {
        installPending_ = std::move(next);
        installTimeout_.start();
    } else {
        pending_ = std::move(next);
        timeout_.start();
    }
    txBytes_ += wire.size();
    if (core_) {
        QTimer::singleShot(0, this, [this, wire, generation = generation_, id] {
            const bool expected = (pending_ && pending_->frame.id == id) ||
                (installPending_ && installPending_->frame.id == id);
            if (generation != generation_ || !core_ || !expected)
                return;
            flint::FrameParser parser;
            parser.feed(wire, [this](flint::Frame frame) {
                if (auto result = core_->process(frame, session_)) {
                    const auto reply = flint::encodeFrame(*result);
                    receive(QByteArray(reinterpret_cast<const char *>(reply.data()), reply.size()));
                }
                drainSimulationEvents();
                scheduleSimulation();
            });
        });
    } else if (usb) {
        if (serial_.write(reinterpret_cast<const char *>(wire.data()), wire.size()) !=
            qint64(wire.size())) {
            fail("Не удалось отправить USB-пакет");
            return;
        }
    } else if (!ble_ ||
        !ble_->write(QByteArray(reinterpret_cast<const char *>(wire.data()), wire.size()))) {
        fail("Не удалось отправить BLE-пакет");
        return;
    }
    if (method == flint::Method::install_commit)
        QTimer::singleShot(0, this, [this] { sendNext(); });
}
void PlatformClient::receive(const QByteArray &bytes, bool usb) {
    rxBytes_ += bytes.size();
    auto &parser = usb ? usbParser_ : parser_;
    parser.feed({reinterpret_cast<const uint8_t *>(bytes.data()), size_t(bytes.size())},
        [this, usb](flint::Frame frame) {
            if (closing_) {
                if (usb && frame.reply && frame.id == closeId_ &&
                    frame.method == flint::Method::session_close) {
                    if (frame.payload.size() >= 2 && flint::u16(frame.payload.data()) == 0)
                        ++closeConfirmed_;
                    finishDisconnect();
                }
                return;
            }
            if (flint::isCompanionEvent(frame)) {
                if (usb && ble_ && ble_->ready())
                    return;
                if (!connected_ || !(capabilities_ & flint::CapCompanionDemand))
                    return;
                flint::Reader event{frame.payload};
                const auto version = event.n(2), kind = event.n(1), generation = event.n(8),
                           state = event.n(1);
                event.n(8); // The SDK owns the consumer identity.
                const auto interval = event.n(), ttl = event.n();
                if (!event.done() || version != flint::CompanionDemandVersion || kind != 1 ||
                    !generation || state > 2 || interval < 1000 || interval > 60000 ||
                    ttl < interval) {
                    fail(tr("Invalid Companion state."));
                    return;
                }
                emit companionState(generation, int(state), int(interval));
                return;
            }
            if (!frame.reply)
                return;
            std::unique_ptr<Pending> done;
            if (installPending_ && frame.id == installPending_->frame.id &&
                frame.method == installPending_->frame.method) {
                installTimeout_.stop();
                done = std::move(installPending_);
            } else if (pending_ && frame.id == pending_->frame.id &&
                frame.method == pending_->frame.method) {
                timeout_.stop();
                done = std::move(pending_);
            } else
                return;
            maxRequestMs_ = std::max(maxRequestMs_, monotonic() - done->sentAt);
            flint::Reader r{frame.payload};
            auto error = flint::Error(r.n(2));
            if (error != flint::Error::ok)
                ++requestErrors_;
            if (!r.valid) {
                fail("Повреждённый ответ");
                return;
            }
            flint::Bytes data(frame.payload.begin() + 2, frame.payload.end());
            done->callback(error, std::move(data));
            QTimer::singleShot(0, this, [this] { sendNext(); });
        });
}
QVariantMap PlatformClient::diagnostics() const {
    return {{"session_close_confirmed", qulonglong(closeConfirmed_)},
        {"session_close_timeouts", qulonglong(closeTimeouts_)}, {"requests", qulonglong(requests_)},
        {"request_errors", qulonglong(requestErrors_)}, {"queue_depth", qulonglong(queue_.size())},
        {"queue_peak", qulonglong(queuePeak_)}, {"queue_rejected", qulonglong(queueRejected_)},
        {"rx_bytes", qulonglong(rxBytes_)}, {"tx_bytes", qulonglong(txBytes_)},
        {"timeouts", qulonglong(timeouts_)}, {"max_queue_ms", qulonglong(maxQueueMs_)},
        {"max_request_ms", qulonglong(maxRequestMs_)}};
}
void PlatformClient::refreshInfo() {
    request(flint::Method::info, {}, [this](auto error, auto data) {
        if (error != flint::Error::ok) {
            emit failure(platformError(error));
            return;
        }
        flint::Reader r{data};
        QVariantMap info;
        info["profile"] = QString::fromStdString(r.text(96));
        info["build"] = QString::fromStdString(r.text(96));
        info["internal"] = qulonglong(r.n());
        info["external"] = qulonglong(r.n());
        info["storage"] = qulonglong(r.n(8));
        info["active"] = QString::fromStdString(r.text(64));
        info["component"] = QString::fromStdString(r.text(64));
        info["epoch"] = qulonglong(r.n(8));
        info["generation"] = qulonglong(r.n(8));
        if (!r.done()) {
            emit failure("Некорректные сведения устройства");
            return;
        }
        info_ = info;
        emit changed();
    });
}
void PlatformClient::refreshBlePolicy() {
    request(flint::Method::settings_get, {}, [this](auto error, auto data) {
        if (error != flint::Error::ok) {
            emit failure(platformError(error));
            return;
        }
        flint::Reader reader{data};
        const auto count = reader.n(2);
        if (count > 128) {
            emit failure(tr("Invalid device settings"));
            return;
        }
        bool enabled = true;
        for (unsigned i = 0; i < count && reader.valid; ++i) {
            const auto key = reader.text(64), value = reader.text(192);
            if (key == "ble_enabled") {
                if (value != "0" && value != "1")
                    reader.valid = false;
                enabled = value == "1";
            }
        }
        if (!reader.done()) {
            emit failure(tr("Invalid device settings"));
            return;
        }
        bleEnabled_ = enabled;
        blePolicyKnown_ = true;
        emit changed();
    });
}
void PlatformClient::setBleEnabled(bool enabled) {
    if (!blePolicySupported() || !blePolicyKnown_ || blePolicyBusy_)
        return;
    flint::Writer writer;
    writer.text("ble_enabled");
    writer.text(enabled ? "1" : "0");
    blePolicyBusy_ = true;
    emit changed();
    request(flint::Method::settings_set, writer.data, [this, enabled](auto error, auto) {
        blePolicyBusy_ = false;
        if (error != flint::Error::ok)
            emit failure(platformError(error));
        else
            bleEnabled_ = enabled;
        emit changed();
    });
}
void PlatformClient::resetTimeSync() {
    ++timeSyncOperation_;
    timeSyncTimeout_.stop();
    if (timeSyncState_ == TimeSyncState::Idle)
        return;
    timeSyncState_ = TimeSyncState::Idle;
    timeSyncError_.clear();
    emit timeSyncChanged();
}
void PlatformClient::completeTimeSync(bool success, const QString &error) {
    const auto generation = generation_, operation = timeSyncOperation_;
    timeSyncTimeout_.stop();
    timeSyncState_ = success ? TimeSyncState::Succeeded : TimeSyncState::Failed;
    timeSyncError_ = error;
    emit timeSyncChanged();
    if (generation != generation_ || operation != timeSyncOperation_ || !connected_)
        return;
    if (!success)
        emit failure(error);
    else
        refreshInfo();
}
void PlatformClient::syncTime() {
    if (!connected_ || timeSyncBusy())
        return;
    const auto generation = generation_, operation = ++timeSyncOperation_;
    timeSyncState_ = TimeSyncState::Pending;
    timeSyncError_.clear();
    timeSyncTimeout_.start();
    emit timeSyncChanged();
    if (generation != generation_ || operation != timeSyncOperation_ || !connected_)
        return;
    flint::Writer w;
    w.n(QDateTime::currentSecsSinceEpoch(), 8);
    request(flint::Method::time_set, w.data, [this, generation, operation](auto error, auto data) {
        if (generation != generation_ || operation != timeSyncOperation_ || !connected_ ||
            !timeSyncBusy())
            return;
        if (error != flint::Error::ok || !data.empty())
            completeTimeSync(false,
                error == flint::Error::ok ? tr("Invalid time status response")
                                          : platformError(error));
        else
            pollTimeStatus(generation, operation);
    });
}
void PlatformClient::pollTimeStatus(uint64_t generation, uint64_t operation) {
    if (generation != generation_ || operation != timeSyncOperation_ || !connected_ ||
        !timeSyncBusy())
        return;
    request(flint::Method::time_status, {}, [this, generation, operation](auto error, auto data) {
        if (generation != generation_ || operation != timeSyncOperation_ || !connected_ ||
            !timeSyncBusy())
            return;
        if (error != flint::Error::ok) {
            completeTimeSync(false, platformError(error));
            return;
        }
        flint::Reader r{data};
        const auto epoch = r.n(8), state = r.n(1), reason = r.n(2);
        if (!r.done() || epoch < 1700000000 || epoch > 4102444800ULL || state > 3 ||
            ((state == 0 || state == 2) && reason != 0) || (state == 3 && reason == 0)) {
            completeTimeSync(false, tr("Invalid time status response"));
            return;
        }
        if (state == 1) {
            QTimer::singleShot(200, this,
                [this, generation, operation] { pollTimeStatus(generation, operation); });
        } else if (state == 3) {
            completeTimeSync(false,
                tr("Core time updated, but RTC write failed: %1")
                    .arg(platformError(flint::Error(reason))));
        } else {
            completeTimeSync(true);
        }
    });
}
void PlatformClient::setOffset(const QString &minutes) {
    flint::Writer w;
    w.text("utc_offset");
    w.text(minutes.toStdString());
    request(flint::Method::settings_set, w.data, [this](auto e, auto) {
        if (e != flint::Error::ok)
            emit failure(platformError(e));
    });
}
void PlatformClient::recover() {
    request(flint::Method::recover, {}, [this](auto e, auto) {
        if (e != flint::Error::ok)
            emit failure(platformError(e));
        else
            refreshInfo();
    });
}

bool PlatformClient::autoSyncTime() const {
    return QSettings().value("connection/autoSyncTime", true).toBool();
}
void PlatformClient::setAutoSyncTime(bool enabled) {
    QSettings().setValue("connection/autoSyncTime", enabled);
    emit changed();
}

void PlatformClient::scheduleSimulation() {
    if (!core_) {
        simulationTick_.stop();
        return;
    }
    const auto deadline = std::min(core_->nextWake(), core_->companionNextWake(session_));
    if (deadline == UINT64_MAX) {
        simulationTick_.stop();
        return;
    }
    const auto at = simulationNow();
    const auto delay =
        deadline <= at ? 0 : std::min<uint64_t>(deadline - at, std::numeric_limits<int>::max());
    simulationTick_.start(int(delay));
}
flint::Error PlatformClient::simulatorInput(flint::InputEvent event) {
    // Interactive preview never injects input into a connected physical watch.
    if (!core_ || !connected_)
        return flint::Error::not_supported;
    const auto error = core_->postInput(event);
    if (error == flint::Error::ok)
        scheduleSimulation();
    return error;
}
void PlatformClient::discardCompanionSamples() {
    std::erase_if(queue_, [](const auto &entry) {
        return entry.frame.method == flint::Method::companion_sample ||
            entry.frame.method == flint::Method::companion_bind;
    });
}
void PlatformClient::drainSimulationEvents() {
    if (!core_)
        return;
    flint::Frame event;
    if (core_->takeReply(session_, event)) {
        const auto bytes = flint::encodeFrame(event);
        receive(QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
        if (!core_)
            return;
    }
    if (core_->takeCompanionEvent(session_, event)) {
        const auto bytes = flint::encodeFrame(event);
        receive(QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    }
}

uint64_t PlatformClient::simulationNow() const {
    const auto now = monotonic();
    return now > UINT64_MAX - simulationOffset_ ? UINT64_MAX : now + simulationOffset_;
}
bool PlatformClient::setVirtualSensorAvailable(int source, bool available) {
    if (!core_ || !sensorSimulation_ || source < 1 || source > 4)
        return false;
    sensorSimulation_->available(uint16_t(source), available);
    scheduleSimulation();
    return true;
}
bool PlatformClient::resetVirtualSensor(int source) {
    if (!core_ || !sensorSimulation_ || source < 1 || source > 4)
        return false;
    sensorSimulation_->reset(uint16_t(source));
    scheduleSimulation();
    return true;
}
bool PlatformClient::setVirtualSensorValue(int source, int channel, double x, double y, double z) {
    constexpr uint8_t masks[]{7, 8, 48, 1};
    if (!core_ || !sensorSimulation_ || source < 1 || source > 4 || channel < 0 || channel >= 6 ||
        !(masks[source - 1] & (1u << channel)) || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(z))
        return false;
    auto values = virtualValues_[source - 1];
    values[channel] = {x, channel >= 4 ? y : 0, channel >= 4 ? z : 0};
    if (sensorSimulation_->setValues(uint16_t(source), values) != flint::Error::ok)
        return false;
    virtualValues_[source - 1] = values;
    scheduleSimulation();
    return true;
}
bool PlatformClient::advanceSimulation(int milliseconds) {
    if (!core_ || milliseconds <= 0 || milliseconds > 86400000 ||
        simulationOffset_ > UINT64_MAX - uint64_t(milliseconds))
        return false;
    simulationOffset_ += uint64_t(milliseconds);
    if (core_->tick())
        emit simulationSceneChanged();
    drainSimulationEvents();
    scheduleSimulation();
    return true;
}

bool PlatformClient::setVirtualSensorState(int source, int state) {
    if (!core_ || !sensorSimulation_ || source < 1 || source > 4 || state < 0 || state > 5)
        return false;
    const auto error = sensorSimulation_->status(uint16_t(source), flint::sensors::Status(state),
        state == int(flint::sensors::Status::valid) ? flint::Error::ok
                                                    : flint::Error::source_unavailable);
    scheduleSimulation();
    return error == flint::Error::ok;
}

void PlatformClient::setVirtualLowEnergy(bool enabled) {
    if (!core_ || !connected_ || virtualLowEnergy_ == enabled)
        return;
    virtualLowEnergy_ = enabled;
    if (core_->tick())
        emit simulationSceneChanged();
    drainSimulationEvents();
    scheduleSimulation();
    emit changed();
}
