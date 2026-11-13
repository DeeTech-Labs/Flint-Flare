#pragma once
#include "PlatformClient.h"
#include <QElapsedTimer>
class Sensors : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool panelActive READ panelActive WRITE setPanelActive NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QVariantMap reading READ reading NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    Q_PROPERTY(bool settingsBusy READ settingsBusy NOTIFY changed)
    Q_PROPERTY(QString settingsMessage READ settingsMessage NOTIFY changed)
public:
    explicit Sensors(PlatformClient *client);
    bool panelActive() const {
        return panelActive_;
    }
    bool enabled() const {
        return enabled_;
    }
    bool busy() const {
        return busy_;
    }
    QVariantList sources() const {
        return sources_;
    }
    QVariantMap reading() const {
        return reading_;
    }
    QString message() const {
        return message_;
    }
    QVariantMap settings() const {
        return settings_;
    }
    bool settingsBusy() const {
        return settingsBusy_;
    }
    QString settingsMessage() const {
        return settingsMessage_;
    }
    Q_INVOKABLE void refreshSettings();
    Q_INVOKABLE void setSetting(const QString &key, const QString &value);
    void setPanelActive(bool active);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void start(int source);
    Q_INVOKABLE void stop();
    Q_INVOKABLE QString stateLabel(int state) const;
    Q_INVOKABLE QString channelLabel(int channel) const;
signals:
    void changed();

private:
    PlatformClient *client_;
    QTimer timer_;
    QElapsedTimer renewal_, lastRead_, sampleReceived_;
    QVariantList sources_;
    QVariantMap reading_;
    QString message_, settingsMessage_, pendingSettingKey_, pendingSettingValue_;
    QVariantMap settings_;
    bool settingsBusy_{};
    uint64_t settingsGeneration_{};
    void readSettings(bool afterSave, const QString &operationError = {});
    void settingsRequest(flint::Method, flint::Bytes, PlatformClient::Callback);
    bool panelActive_{}, enabled_{}, busy_{}, leaseRequested_{};
    uint64_t generation_{};
    uint16_t source_{};
    uint8_t channels_{};
    uint32_t interval_{500}, staleAfter_{};
    void request(flint::Method, flint::Bytes, PlatformClient::Callback);
    void lease(bool renew);
    void poll();
};
