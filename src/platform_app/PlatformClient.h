#pragma once
#include "device/BleTransport.h"
#include <QObject>
#include <QSerialPort>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantList>
#include <deque>
#include <flint/core.h>
#include <memory>
class PlatformClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autoSyncTime READ autoSyncTime WRITE setAutoSyncTime NOTIFY changed)
    Q_PROPERTY(bool connecting READ connecting NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString transport READ transport NOTIFY changed)
    Q_PROPERTY(bool simulated READ simulated NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap info READ info NOTIFY changed)
    Q_PROPERTY(QVariantList ports READ ports NOTIFY changed)
    Q_PROPERTY(QVariantList bleDevices READ bleDevices NOTIFY changed)
    Q_PROPERTY(QString pairingCode READ pairingCode NOTIFY changed)
    Q_PROPERTY(QString watchPairingCode READ watchPairingCode NOTIFY changed)
    Q_PROPERTY(bool localTrustChannel READ localTrustChannel NOTIFY changed)
public:
    using Callback = std::function<void(flint::Error, flint::Bytes)>;
    explicit PlatformClient(QObject *parent = nullptr);
    bool autoSyncTime() const;
    void setAutoSyncTime(bool enabled);
    bool connecting() const {
        return !closing_ && !connected_ && (serial_.isOpen() || (ble_ && ble_->active()));
    }
    bool connected() const {
        return connected_;
    }
    uint64_t capabilities() const {
        return capabilities_;
    }
    bool simulated() const {
        return bool(core_);
    }
    QString transport() const {
        return core_ ? "SIMULATOR" : ble_ && ble_->ready() ? "BLE" : serial_.isOpen() ? "USB" : "";
    }
    QString status() const {
        return status_;
    }
    QVariantMap info() const {
        return info_;
    }
    QVariantList ports() const;
    QVariantList bleDevices() const;
    QString pairingCode() const;
    QString watchPairingCode() const {
        return watchPairingCode_;
    }
    bool localTrustChannel() const {
        return !closing_ && serial_.isOpen();
    }
    void request(flint::Method method, flint::Bytes payload, Callback callback);
    void discardCompanionSamples();
    Q_INVOKABLE QVariantMap diagnostics() const;
    flint::Core *simulator() {
        return core_.get();
    }
    flint::Error simulatorInput(flint::InputEvent event);
    Q_INVOKABLE void startSimulator();
    Q_INVOKABLE void connectUsb(const QString &port);
    Q_INVOKABLE void scanBle();
    Q_INVOKABLE void connectBle(const QString &address);
    Q_INVOKABLE void confirmPairing(bool accepted);
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void refreshInfo();
    Q_INVOKABLE void syncTime();
    Q_INVOKABLE void setOffset(const QString &minutes);
    Q_INVOKABLE void recover();
    Q_INVOKABLE void pairing(const QString &action);
signals:
    void changed();
    void ready();
    void disconnected();
    void failure(const QString &message);
    void simulationSceneChanged();
    void companionState(qulonglong generation, int state, int intervalMs);

private:
    friend struct PlatformClientTestAccess;
    struct Pending {
        flint::Frame frame;
        Callback callback;
        uint64_t queuedAt{};
        uint64_t sentAt{};
    };
    QSerialPort serial_;
    std::unique_ptr<BleTransport> ble_;
    QTimer timeout_;
    QTimer pairingPoll_;
    QTimer simulationTick_;
    QTimer closeTimeout_;
    QTimer installTimeout_;
    std::function<void()> afterClose_;
    bool closing_{};
    bool finishingDisconnect_{};
    uint32_t closeId_{};
    uint64_t closeConfirmed_{}, closeTimeouts_{};
    QString watchPairingCode_;
    flint::FrameParser parser_;
    flint::FrameParser usbParser_;
    std::deque<Pending> queue_;
    std::unique_ptr<Pending> pending_;
    std::unique_ptr<Pending> installPending_;
    uint32_t nextId_{};
    uint64_t generation_{};
    uint64_t capabilities_{};
    uint64_t requests_{}, requestErrors_{}, queueRejected_{}, queuePeak_{}, rxBytes_{}, txBytes_{},
        timeouts_{}, maxQueueMs_{}, maxRequestMs_{};
    std::unique_ptr<flint::Core> core_;
    std::unique_ptr<QTemporaryDir> simulation_;
    flint::Session session_{true, false, 1};
    bool connected_{};
    QString status_{"Отключено"};
    QVariantMap info_;
    void sendNext();
    void receive(const QByteArray &bytes, bool usb = false);
    void handshake();
    void fail(const QString &message);
    void ensureBle();
    void scheduleSimulation();
    void drainSimulationEvents();
    void resetConnection(std::function<void()> afterClose = {});
    void finishDisconnect();
    void startSimulatorNow();
    void openUsb(const QString &port);
    void openBle(const QString &address);
};
QString platformError(flint::Error error);
