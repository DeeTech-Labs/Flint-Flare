#include "CompanionService.h"
#include <QFile>
#include <QPointer>
#include <QRegularExpression>

CompanionService::CompanionService(PlatformClient *client) : client_(client) {
    updateStatus();
    timer_.setInterval(flint::CompanionSampleIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &CompanionService::sample);
    connect(client, &PlatformClient::ready, this, [this] { subscribe(); });
    connect(client, &PlatformClient::companionState, this,
        [this](qulonglong generation, int state, int interval) {
            if (!allowed_ || !client_->connected())
                return;
            stopSampling();
            demandGeneration_ = generation;
            demand_ = state;
            timer_.setInterval(interval);
            if (state != int(flint::CompanionDemand::active)) {
                updateStatus();
                return;
            }
#ifdef Q_OS_LINUX
            flint::Writer binding;
            binding.n(flint::CompanionDemandVersion, 2);
            binding.n(1, 1);
            binding.n(demandGeneration_, 8);
            client_->request(flint::Method::companion_bind, binding.data,
                [this, guard = QPointer<CompanionService>(this), token = generation_](
                    auto error, auto) {
                    if (!guard || token != generation_ || !allowed_ || !client_->connected())
                        return;
                    bound_ = error == flint::Error::ok;
                    if (bound_) {
                        updateStatus();
                        timer_.start();
                        sample();
                    } else {
                        status_ = platformError(error);
                        emit changed();
                    }
                });
#else
            updateStatus();
#endif
        });
    connect(client, &PlatformClient::disconnected, this, [this] {
        stopSampling();
        allowed_ = false;
        demand_ = -1;
        updateStatus();
    });
}
void CompanionService::stopSampling() {
    ++generation_;
    timer_.stop();
    total_ = idle_ = 0;
    bound_ = samplePending_ = delivered_ = false;
    client_->discardCompanionSamples();
}
void CompanionService::updateStatus() {
    if (!allowed_)
        status_ = tr("CPU/RAM sharing is off");
    else if (!client_->connected())
        status_ = tr("Device disconnected");
    else if (!(client_->capabilities() & flint::CapCompanionDemand))
        status_ = tr("Device does not support Companion demand");
#ifndef Q_OS_LINUX
    else
        status_ = tr("CPU/RAM source is supported on Linux");
#else
    else if (demand_ == int(flint::CompanionDemand::permission_denied))
        status_ = tr("Ignite has no CPU/RAM permission");
    else if (demand_ != int(flint::CompanionDemand::active))
        status_ = tr("Waiting for a consumer");
    else if (!delivered_)
        status_ = tr("Preparing CPU/RAM source");
    else
        status_ = tr("CPU/RAM sharing is active");
#endif
    emit changed();
}
void CompanionService::subscribe() {
    stopSampling();
    demand_ = -1;
    updateStatus();
    if (!allowed_ || !client_->connected() ||
        !(client_->capabilities() & flint::CapCompanionDemand))
        return;
    flint::Writer request;
    request.n(flint::CompanionDemandVersion, 2);
    request.n(1, 1);
    client_->request(flint::Method::companion_subscribe, request.data,
        [this, guard = QPointer<CompanionService>(this), token = generation_](auto error, auto) {
            if (!guard || token != generation_ || error == flint::Error::ok)
                return;
            status_ = platformError(error);
            emit changed();
        });
}
void CompanionService::setMetricsAllowed(bool value) {
    if (allowed_ == value)
        return;
    stopSampling();
    allowed_ = value;
    demand_ = -1;
    if (value) {
        subscribe();
    } else {
        if (client_->connected() && (client_->capabilities() & flint::CapCompanionDemand)) {
            flint::Writer request;
            request.n(flint::CompanionDemandVersion, 2);
            request.n(0, 1);
            client_->request(flint::Method::companion_subscribe, request.data, [](auto, auto) {});
        }
        updateStatus();
    }
}
void CompanionService::sample() {
    if (!allowed_ || !bound_ || samplePending_ || !client_->connected() ||
        demand_ != int(flint::CompanionDemand::active))
        return;
#ifdef Q_OS_LINUX
    ++sampleAttempts_;
    const auto unavailable = [this] {
        status_ = tr("CPU/RAM source unavailable");
        emit changed();
    };

    QFile stat("/proc/stat"), memory("/proc/meminfo");
    if (!stat.open(QIODevice::ReadOnly) || !memory.open(QIODevice::ReadOnly)) {
        unavailable();
        return;
    }
    auto fields = QString::fromUtf8(stat.readLine()).simplified().split(' ');
    if (fields.size() < 5) {
        unavailable();
        return;
    }
    quint64 total = 0;
    for (int i = 1; i < fields.size() && i <= 8; ++i)
        total += fields[i].toULongLong();
    auto idle = fields[4].toULongLong() + (fields.size() > 5 ? fields[5].toULongLong() : 0);
    double cpu = total > total_ ? 100.0 * (1.0 - double(idle - idle_) / double(total - total_)) : 0;
    const bool baseline = !total_ || total <= total_ || idle < idle_;
    total_ = total;
    idle_ = idle;
    if (baseline)
        return;
    auto text = QString::fromUtf8(memory.readAll());
    auto all = QRegularExpression("MemTotal:\\s+(\\d+)").match(text),
         free = QRegularExpression("MemAvailable:\\s+(\\d+)").match(text);
    if (!all.hasMatch() || !free.hasMatch() || !all.captured(1).toDouble()) {
        unavailable();
        return;
    }
    auto ram = 100 * (1 - free.captured(1).toDouble() / all.captured(1).toDouble());
    flint::Writer metrics;
    metrics.n(flint::CompanionDemandVersion, 2);
    metrics.n(1, 1);
    metrics.n(demandGeneration_, 8);
    metrics.n(qBound(0, int(cpu * 100), 10000));
    metrics.n(qBound(0, int(ram * 100), 10000));
    if (!allowed_)
        return;

    ++metricSamples_;
    samplePending_ = true;
    client_->request(flint::Method::companion_sample, metrics.data,
        [this, guard = QPointer<CompanionService>(this), token = generation_](auto error, auto) {
            if (!guard || token != generation_)
                return;
            samplePending_ = false;
            if (error == flint::Error::stale_handle || error == flint::Error::permission_denied) {
                stopSampling();
                demand_ = -1;
            }
            if (error == flint::Error::ok) {
                delivered_ = true;
                updateStatus();
            } else {
                status_ = platformError(error);
                emit changed();
            }
        });
#endif
}
