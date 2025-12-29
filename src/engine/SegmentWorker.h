/**
 * @file SegmentWorker.h
 * @brief Worker thread for downloading a single segment
 *
 * Each SegmentWorker runs in its own thread and owns a CURL handle
 * for downloading its assigned byte range. Workers communicate with
 * the SegmentScheduler via Qt signals for progress and completion.
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_SEGMENTWORKER_H
#define OPENIDM_SEGMENTWORKER_H

#include <QObject>
#include <QThread>
#include <QFile>
#include <QMutex>
#include <QWaitCondition>

#include <atomic>
#include <memory>

#include "DownloadTypes.h"
#include "CurlWrapper.h"
#include "SpeedCalculator.h"

namespace OpenIDM {

/**
 * @brief Downloads a single segment of a file
 *
 * SegmentWorker is designed to be used with QThread via moveToThread().
 * It handles:
 * - HTTP range requests
 * - Writing to temporary .part files
 * - Progress reporting
 * - Automatic retries with exponential backoff
 * - Graceful pause/resume
 */
class SegmentWorker : public QObject {
    Q_OBJECT

    Q_PROPERTY(int segmentId READ segmentId CONSTANT)
    Q_PROPERTY(SegmentState state READ state NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressUpdated)
    Q_PROPERTY(double speed READ speed NOTIFY speedUpdated)

public:
    /**
     * @brief Constructor
     * @param segmentId Unique ID for this segment within the download
     * @param parent QObject parent (should be nullptr for moveToThread)
     */
    explicit SegmentWorker(int segmentId, QObject* parent = nullptr);
    ~SegmentWorker() override;

    // Non-copyable, non-movable (QObject constraint)
    SegmentWorker(const SegmentWorker&) = delete;
    SegmentWorker& operator=(const SegmentWorker&) = delete;

    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Configure the segment to download
     * @param url Download URL
     * @param startByte First byte (inclusive)
     * @param endByte Last byte (inclusive)
     * @param partFilePath Path for temporary .part file
     * @param resumeOffset Bytes already downloaded (for resume)
     */
    void configure(const QUrl& url, qint64 startByte, qint64 endByte,
                   const QString& partFilePath, qint64 resumeOffset = 0);

    /**
     * @brief Set maximum download speed
     */
    void setSpeedLimit(qint64 bytesPerSecond);

    /**
     * @brief Set HTTP headers (User-Agent, Referer, etc.)
     */
    void setHeaders(const QStringList& headers);

    // ═══════════════════════════════════════════════════════════════════════════
    // Property Accessors
    // ═══════════════════════════════════════════════════════════════════════════

    [[nodiscard]] int segmentId() const noexcept { return m_segmentId; }
    [[nodiscard]] SegmentState state() const noexcept { return m_state.load(); }
    [[nodiscard]] qint64 startByte() const noexcept { return m_startByte; }
    [[nodiscard]] qint64 endByte() const noexcept { return m_endByte; }
    [[nodiscard]] qint64 downloadedBytes() const noexcept { return m_downloadedBytes.load(); }
    [[nodiscard]] qint64 remainingBytes() const noexcept;
    [[nodiscard]] double progress() const noexcept;
    [[nodiscard]] double speed() const;

    /**
     * @brief Get current segment info
     */
    [[nodiscard]] SegmentInfo segmentInfo() const;

    /**
     * @brief Check if this segment can be split (for work stealing)
     */
    [[nodiscard]] bool canSplit() const noexcept;

    /**
     * @brief Split this segment and return new segment info
     *
     * Atomically splits the segment at the midpoint of remaining bytes.
     * Updates this worker's endByte and returns info for the new segment.
     *
     * @return New segment info, or empty if split not possible
     */
    [[nodiscard]] std::optional<SegmentInfo> split();

public slots:
    /**
     * @brief Start downloading
     *
     * Should be called after moveToThread() and thread start.
     */
    void start();

    /**
     * @brief Pause downloading
     *
     * Gracefully stops the current transfer, preserving progress.
     */
    void pause();

    /**
     * @brief Resume downloading
     */
    void resume();

    /**
     * @brief Stop downloading and cleanup
     *
     * Called when the download is cancelled or worker is being destroyed.
     */
    void stop();

    /**
     * @brief Update the end byte (for segment shrinking during split)
     */
    void updateEndByte(qint64 newEndByte);

signals:
    /**
     * @brief Emitted when segment state changes
     */
    void stateChanged(SegmentState newState);

    /**
     * @brief Emitted periodically with progress update
     */
    void progressUpdated(int segmentId, qint64 downloadedBytes, qint64 totalBytes);

    /**
     * @brief Emitted when speed is recalculated
     */
    void speedUpdated(int segmentId, double bytesPerSecond);

    /**
     * @brief Emitted when bytes are received
     */
    void bytesReceived(int segmentId, qint64 bytes);

    /**
     * @brief Emitted when segment completes successfully
     */
    void completed(int segmentId);

    /**
     * @brief Emitted on error
     */
    void error(int segmentId, const QString& message);

    /**
     * @brief Emitted when segment is paused
     */
    void paused(int segmentId);

    /**
     * @brief Request to exit the thread event loop
     */
    void finished();

private slots:
    void onDataReceived(qint64 bytes);
    void onProgressUpdated(qint64 downloadTotal, qint64 downloadNow);

private:
    void setState(SegmentState newState);
    void performDownload();
    bool shouldRetry(const CurlResult& result);
    void scheduleRetry();
    bool writeData(std::span<const char> data);
    void closeFile();

    int m_segmentId;
    QUrl m_url;
    qint64 m_startByte = 0;
    qint64 m_endByte = 0;
    QString m_partFilePath;
    QStringList m_customHeaders;

    std::atomic<SegmentState> m_state{SegmentState::Pending};
    std::atomic<qint64> m_downloadedBytes{0};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_pauseRequested{false};

    std::unique_ptr<CurlEasyHandle> m_curl;
    std::unique_ptr<QFile> m_file;
    std::unique_ptr<SpeedCalculator> m_speedCalc;

    int m_retryCount = 0;
    qint64 m_speedLimit = 0;

    mutable QMutex m_mutex;
    QWaitCondition m_pauseCondition;
};

/**
 * @brief Factory for creating SegmentWorker with its own thread
 */
class SegmentWorkerThread : public QObject {
    Q_OBJECT

public:
    explicit SegmentWorkerThread(int segmentId, QObject* parent = nullptr);
    ~SegmentWorkerThread() override;

    /**
     * @brief Get the worker instance
     */
    [[nodiscard]] SegmentWorker* worker() const { return m_worker; }

    /**
     * @brief Get the thread instance
     */
    [[nodiscard]] QThread* thread() const { return m_thread; }

    /**
     * @brief Start the worker thread
     */
    void start();

    /**
     * @brief Stop the worker and thread
     */
    void stop();

    /**
     * @brief Wait for thread to finish
     */
    bool wait(unsigned long time = ULONG_MAX);

private:
    QThread* m_thread = nullptr;
    SegmentWorker* m_worker = nullptr;
};

} // namespace OpenIDM

#endif // OPENIDM_SEGMENTWORKER_H
