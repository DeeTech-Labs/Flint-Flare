#pragma once
#include <cstdint>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include "BluezDiscoverySession.h"
#include "BluezPairingAgent.h"
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QTimer>
#include <QVariantList>

// GATT lifecycle lives here; DeviceSession owns the shared command protocol.
class BleTransport : public QObject {
    Q_OBJECT
public:
    explicit BleTransport(QObject* parent = nullptr);
    ~BleTransport() override { close(); }
    static bool isCandidate(const QBluetoothDeviceInfo& info);
    QVariantList devices() const;
    bool scanning() const { return m_discovery && m_discovery->isActive(); }
    bool active() const { return m_controller != nullptr; }
    bool ready() const { return m_ready; }
    QString pairingCode() const { return m_pairingAgent.confirmation(); }
    void confirmPairing(bool accepted) { m_pairingAgent.confirm(accepted); }
    void scan();
    void open(const QString& id);
    void close();
    bool write(const QByteArray& frame);
    bool canWrite() const { return m_ready && m_service && m_outgoing.isEmpty(); }
signals:
    void pairingCodeChanged();
    void devicesChanged();
    void readyChanged();
    void writable();
    void received(const QByteArray& bytes);
    void error(const QString& message);
private:
    void ensureDiscovery();
    void discoverService();
    void subscribe();
    void writeNext();
    uint64_t m_generation = 0;
    QByteArray m_outgoing;
    bool m_writeInFlight = false;
    BluezDiscoverySession m_bluezDiscovery;
    BluezPairingAgent m_pairingAgent;
    QBluetoothDeviceDiscoveryAgent* m_discovery = nullptr;
    QList<QBluetoothDeviceInfo> m_devices;
    QBluetoothLocalDevice* m_pairer = nullptr;
    QLowEnergyController* m_controller = nullptr;
    QLowEnergyService* m_service = nullptr;
    QLowEnergyCharacteristic m_rx;
    QLowEnergyCharacteristic m_tx;
    QTimer m_timeout;
    bool m_ready = false;
};
