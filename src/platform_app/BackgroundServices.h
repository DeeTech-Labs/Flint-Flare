#pragma once
#include "PlatformClient.h"
#include <QAbstractListModel>
class BackgroundServices : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool panelActive READ panelActive WRITE setPanelActive NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool updating READ updating NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel *model READ model CONSTANT)
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(QVariantMap policy READ policy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit BackgroundServices(PlatformClient *client);
    bool panelActive() const {
        return active_;
    }
    QAbstractItemModel *model() {
        return this;
    }
    bool updating() const {
        return mutating_;
    }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool busy() const {
        return fetching_ || mutating_;
    }
    QVariantList services() const {
        return services_;
    }
    QVariantMap policy() const {
        return policy_;
    }
    QString message() const;
    void setPanelActive(bool active);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void control(int action, const QString &package, const QString &component);
    Q_INVOKABLE void grant(const QString &package, const QString &permission, bool enabled);
    Q_INVOKABLE QString stateLabel(int state) const;
    Q_INVOKABLE QString reasonLabel(int reason) const;
signals:
    void changed();

private:
    PlatformClient *client_;
    QTimer timer_;
    QVariantList services_, incoming_;
    QVariantMap policy_, incomingPolicy_;
    enum class Notice {
        none,
        disconnected,
        unavailable,
        invalidResponse,
        applying,
        confirmed,
        failure
    };
    Notice notice_{}, queryNotice_{};
    flint::Error error_{flint::Error::ok}, queryError_{flint::Error::ok};
    bool active_{}, fetching_{}, mutating_{};
    uint64_t connectionGeneration_{}, fetchGeneration_{};
    void page(uint16_t offset, uint64_t generation);
    void replaceRows(QVariantList rows);
    bool known(const QString &package, const QString &component = {}) const;
    void mutate(flint::Method, flint::Bytes);
};
