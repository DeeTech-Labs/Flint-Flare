#pragma once
#include "PlatformClient.h"
class CompanionService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool metricsAllowed READ metricsAllowed WRITE setMetricsAllowed NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit CompanionService(PlatformClient *client);
    bool metricsAllowed() const {
        return allowed_;
    }
    void setMetricsAllowed(bool value);
    QString status() const {
        return status_;
    }
    Q_INVOKABLE QVariantMap diagnostics() const {
        return {{"sample_attempts", qulonglong(sampleAttempts_)},
            {"permission_requests", qulonglong(permissionRequests_)},
            {"metric_samples", qulonglong(metricSamples_)}};
    }
signals:
    void changed();

private:
    PlatformClient *client_;
    QTimer timer_;
    bool allowed_{};
    QString status_;
    quint64 total_{}, idle_{};
    quint64 sampleAttempts_{}, permissionRequests_{}, metricSamples_{}, generation_{};
    quint64 demandGeneration_{};
    int demand_{-1};
    bool bound_{}, samplePending_{}, delivered_{};
    void sample();
    void subscribe();
    void stopSampling();
    void updateStatus();
};
