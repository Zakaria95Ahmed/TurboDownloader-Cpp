/**
 * @file SpeedCalculator.h
 * @brief Smoothed speed and ETA calculation for downloads
 *
 * Implements a rolling window average for speed calculation to provide
 * stable, user-friendly speed readings that don't fluctuate wildly.
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_SPEEDCALCULATOR_H
#define OPENIDM_SPEEDCALCULATOR_H

#include <QObject>
#include <QElapsedTimer>
#include <QDateTime>
#include <QMutex>

#include <deque>
#include <optional>

#include "DownloadTypes.h"

namespace OpenIDM {

/**
 * @brief Sample point for speed calculation
 */
struct SpeedSample {
    QDateTime timestamp;
    qint64 bytes;
    qint64 totalBytes;
};

/**
 * @brief Calculates download speed using a rolling window average
 *
 * Thread-safe implementation that can be called from multiple threads.
 * Uses exponential moving average for smoother readings.
 */
class SpeedCalculator : public QObject {
    Q_OBJECT

    Q_PROPERTY(double currentSpeed READ currentSpeed NOTIFY speedUpdated)
    Q_PROPERTY(double averageSpeed READ averageSpeed NOTIFY speedUpdated)
    Q_PROPERTY(qint64 eta READ eta NOTIFY etaUpdated)
    Q_PROPERTY(qint64 peakSpeed READ peakSpeed NOTIFY speedUpdated)

public:
    /**
     * @brief Constructor
     * @param windowSeconds Size of rolling window in seconds
     * @param parent QObject parent
     */
    explicit SpeedCalculator(int windowSeconds = Config::SPEED_SAMPLE_WINDOW_SECONDS,
                            QObject* parent = nullptr);

    ~SpeedCalculator() override = default;

    // Non-copyable
    SpeedCalculator(const SpeedCalculator&) = delete;
    SpeedCalculator& operator=(const SpeedCalculator&) = delete;

    /**
     * @brief Reset all calculations
     */
    void reset();

    /**
     * @brief Record bytes downloaded
     * @param bytes Bytes downloaded since last update
     * @param totalBytesToDownload Total file size (for ETA calculation)
     * @param totalBytesDownloaded Total bytes downloaded so far
     */
    void addBytes(qint64 bytes, qint64 totalBytesToDownload = -1,
                  qint64 totalBytesDownloaded = 0);

    /**
     * @brief Get current speed (smoothed, bytes/sec)
     */
    [[nodiscard]] double currentSpeed() const;

    /**
     * @brief Get overall average speed since start (bytes/sec)
     */
    [[nodiscard]] double averageSpeed() const;

    /**
     * @brief Get peak speed observed (bytes/sec)
     */
    [[nodiscard]] qint64 peakSpeed() const { return m_peakSpeed; }

    /**
     * @brief Get estimated time remaining in seconds
     * @return Seconds remaining, or -1 if unknown
     */
    [[nodiscard]] qint64 eta() const;

    /**
     * @brief Get formatted speed string (e.g., "5.2 MB/s")
     */
    [[nodiscard]] QString formattedSpeed() const;

    /**
     * @brief Get formatted ETA string (e.g., "2h 15m")
     */
    [[nodiscard]] QString formattedEta() const;

    /**
     * @brief Get total bytes downloaded
     */
    [[nodiscard]] qint64 totalBytesDownloaded() const { return m_totalBytesDownloaded; }

    /**
     * @brief Get elapsed time since start
     */
    [[nodiscard]] qint64 elapsedMs() const { return m_elapsedTimer.elapsed(); }

    /**
     * @brief Get statistics snapshot
     */
    [[nodiscard]] DownloadStats getStats(const QString& downloadId) const;

signals:
    /**
     * @brief Emitted when speed is recalculated
     */
    void speedUpdated(double speed);

    /**
     * @brief Emitted when ETA changes significantly
     */
    void etaUpdated(qint64 etaSeconds);

private:
    void pruneOldSamples();
    double calculateRollingSpeed() const;

    mutable QMutex m_mutex;

    int m_windowSeconds;
    std::deque<SpeedSample> m_samples;

    QElapsedTimer m_elapsedTimer;
    bool m_started = false;

    qint64 m_totalBytesDownloaded = 0;
    qint64 m_totalBytesToDownload = -1;

    double m_currentSpeed = 0.0;
    double m_averageSpeed = 0.0;
    qint64 m_peakSpeed = 0;

    // For exponential moving average
    static constexpr double EMA_ALPHA = 0.3;  // Smoothing factor
    double m_emaSpeed = 0.0;

    qint64 m_lastEta = -1;
};

/**
 * @brief Aggregate speed calculator for multiple downloads
 *
 * Combines statistics from multiple SpeedCalculators for global display.
 */
class AggregateSpeedCalculator : public QObject {
    Q_OBJECT

    Q_PROPERTY(double totalSpeed READ totalSpeed NOTIFY totalSpeedUpdated)
    Q_PROPERTY(int activeDownloads READ activeDownloads NOTIFY activeDownloadsChanged)

public:
    explicit AggregateSpeedCalculator(QObject* parent = nullptr);

    /**
     * @brief Register a calculator for tracking
     */
    void registerCalculator(const QString& downloadId, SpeedCalculator* calculator);

    /**
     * @brief Unregister a calculator
     */
    void unregisterCalculator(const QString& downloadId);

    /**
     * @brief Get total combined speed
     */
    [[nodiscard]] double totalSpeed() const;

    /**
     * @brief Get number of active downloads
     */
    [[nodiscard]] int activeDownloads() const;

    /**
     * @brief Get formatted total speed
     */
    [[nodiscard]] QString formattedTotalSpeed() const;

signals:
    void totalSpeedUpdated(double speed);
    void activeDownloadsChanged(int count);

private slots:
    void onSpeedUpdated();

private:
    mutable QMutex m_mutex;
    QHash<QString, SpeedCalculator*> m_calculators;
};

} // namespace OpenIDM

#endif // OPENIDM_SPEEDCALCULATOR_H
