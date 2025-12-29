/**
 * @file DownloadTask.h
 * @brief Single download lifecycle management
 *
 * DownloadTask represents a single file download and manages its entire
 * lifecycle from queued to completed. It coordinates the SegmentScheduler,
 * handles file merging, and emits progress signals for the UI.
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_DOWNLOADTASK_H
#define OPENIDM_DOWNLOADTASK_H

#include <QObject>
#include <QUrl>
#include <QTimer>

#include <memory>

#include "DownloadTypes.h"
#include "SegmentScheduler.h"

namespace OpenIDM {

// Forward declarations
class PersistenceManager;
class StreamingResolver;

/**
 * @brief Manages the complete lifecycle of a single download
 *
 * State machine:
 *   Queued -> Resolving -> Connecting -> Downloading -> Merging -> Completed
 *                 |            |              |
 *                 v            v              v
 *              Error        Error          Error
 *                 ^            ^              ^
 *                 |            |              |
 *              Paused  <--  Paused  <--   Paused
 */
class DownloadTask : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QUrl url READ url CONSTANT)
    Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY totalSizeChanged)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY progressUpdated)
    Q_PROPERTY(double progress READ progress NOTIFY progressUpdated)
    Q_PROPERTY(double speed READ speed NOTIFY speedUpdated)
    Q_PROPERTY(qint64 eta READ eta NOTIFY etaUpdated)
    Q_PROPERTY(DownloadState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorOccurred)

public:
    /**
     * @brief Create a new download task
     * @param info Initial download information
     * @param parent QObject parent
     */
    explicit DownloadTask(const DownloadInfo& info, QObject* parent = nullptr);

    /**
     * @brief Create from existing info (for resume)
     */
    static std::unique_ptr<DownloadTask> fromInfo(const DownloadInfo& info,
                                                   QObject* parent = nullptr);

    ~DownloadTask() override;

    // Non-copyable
    DownloadTask(const DownloadTask&) = delete;
    DownloadTask& operator=(const DownloadTask&) = delete;

    // ═══════════════════════════════════════════════════════════════════════════
    // Property Accessors
    // ═══════════════════════════════════════════════════════════════════════════

    [[nodiscard]] QString id() const { return m_info.id; }
    [[nodiscard]] QUrl url() const { return m_info.originalUrl; }
    [[nodiscard]] QString fileName() const { return m_info.fileName; }
    [[nodiscard]] QString savePath() const { return m_info.savePath; }
    [[nodiscard]] QString fullFilePath() const { return m_info.fullFilePath(); }
    [[nodiscard]] qint64 totalSize() const { return m_info.totalSize; }
    [[nodiscard]] qint64 downloadedBytes() const { return m_info.downloadedBytes; }
    [[nodiscard]] double progress() const { return m_info.progress(); }
    [[nodiscard]] double speed() const;
    [[nodiscard]] qint64 eta() const;
    [[nodiscard]] DownloadState state() const { return m_info.state; }
    [[nodiscard]] QString errorMessage() const { return m_info.errorMessage; }
    [[nodiscard]] DownloadPriority priority() const { return m_info.priority; }

    /**
     * @brief Get complete download information
     */
    [[nodiscard]] DownloadInfo info() const { return m_info; }

    /**
     * @brief Get real-time statistics
     */
    [[nodiscard]] DownloadStats stats() const;

    /**
     * @brief Check if download can be started
     */
    [[nodiscard]] bool canStart() const;

    /**
     * @brief Check if download can be paused
     */
    [[nodiscard]] bool canPause() const;

    /**
     * @brief Check if download can be resumed
     */
    [[nodiscard]] bool canResume() const;

    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Set save path
     */
    void setSavePath(const QString& path);

    /**
     * @brief Set file name
     */
    void setFileName(const QString& name);

    /**
     * @brief Set priority
     */
    void setPriority(DownloadPriority priority);

    /**
     * @brief Set maximum segments
     */
    void setMaxSegments(int max);

    /**
     * @brief Set speed limit (bytes/sec, 0 = unlimited)
     */
    void setSpeedLimit(qint64 bytesPerSecond);

    /**
     * @brief Set persistence manager for state saving
     */
    void setPersistenceManager(PersistenceManager* persistence);

    /**
     * @brief Set streaming resolver for URL resolution
     */
    void setStreamingResolver(StreamingResolver* resolver);

public slots:
    /**
     * @brief Start the download
     */
    void start();

    /**
     * @brief Pause the download
     */
    void pause();

    /**
     * @brief Resume a paused download
     */
    void resume();

    /**
     * @brief Cancel and cleanup
     */
    void cancel();

    /**
     * @brief Retry a failed download
     */
    void retry();

signals:
    /**
     * @brief State has changed
     */
    void stateChanged(DownloadState newState);

    /**
     * @brief Progress update
     */
    void progressUpdated(qint64 downloaded, qint64 total, double percent);

    /**
     * @brief Speed update
     */
    void speedUpdated(double bytesPerSecond);

    /**
     * @brief ETA update
     */
    void etaUpdated(qint64 seconds);

    /**
     * @brief File name changed (e.g., from Content-Disposition)
     */
    void fileNameChanged(const QString& name);

    /**
     * @brief Total size determined
     */
    void totalSizeChanged(qint64 size);

    /**
     * @brief Download completed successfully
     */
    void completed();

    /**
     * @brief Error occurred
     */
    void errorOccurred(const QString& message);

    /**
     * @brief Download was cancelled
     */
    void cancelled();

    /**
     * @brief Info changed (for persistence)
     */
    void infoChanged(const DownloadInfo& info);

private slots:
    void onSchedulerProgress(qint64 downloaded, qint64 total, double percent);
    void onSchedulerSpeed(double bytesPerSecond);
    void onSchedulerCompleted();
    void onSchedulerError(int segmentId, const QString& message);
    void onSchedulerSegmentsChanged(const std::vector<SegmentInfo>& segments);
    void onUrlResolved(const QUrl& resolvedUrl);
    void onResolveError(const QString& error);
    void onPersistenceTimer();

private:
    void setState(DownloadState newState);
    void resolveUrl();
    void initializeScheduler();
    void startDownload();
    void mergeSegments();
    void cleanup(bool deleteFiles);
    void saveState();

    DownloadInfo m_info;

    std::unique_ptr<SegmentScheduler> m_scheduler;
    PersistenceManager* m_persistence = nullptr;
    StreamingResolver* m_streamingResolver = nullptr;

    QTimer* m_persistenceTimer = nullptr;

    int m_maxSegments = Config::DEFAULT_SEGMENTS;
    qint64 m_speedLimit = 0;

    bool m_urlResolved = false;
};

} // namespace OpenIDM

#endif // OPENIDM_DOWNLOADTASK_H
