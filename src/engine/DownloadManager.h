/**
 * @file DownloadManager.h
 * @brief Global download orchestrator and queue manager
 *
 * DownloadManager is the main entry point for the download engine.
 * It manages all downloads, handles queue logic, and provides the
 * interface for the UI layer.
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_DOWNLOADMANAGER_H
#define OPENIDM_DOWNLOADMANAGER_H

#include <QObject>
#include <QUrl>
#include <QHash>
#include <QMutex>

#include <memory>
#include <vector>

#include "DownloadTypes.h"
#include "DownloadTask.h"
#include "SpeedCalculator.h"

namespace OpenIDM {

// Forward declarations
class PersistenceManager;
class StreamingResolver;

/**
 * @brief Global download manager - singleton orchestrator
 *
 * Responsibilities:
 * - Manage download queue
 * - Enforce concurrency limits
 * - Global bandwidth throttling
 * - Persistence coordination
 * - Statistics aggregation
 */
class DownloadManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY queuedCountChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(double totalSpeed READ totalSpeed NOTIFY totalSpeedUpdated)
    Q_PROPERTY(int maxConcurrent READ maxConcurrent WRITE setMaxConcurrent NOTIFY maxConcurrentChanged)

public:
    /**
     * @brief Get singleton instance
     */
    static DownloadManager* instance();

    ~DownloadManager() override;

    // Non-copyable
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    // ═══════════════════════════════════════════════════════════════════════════
    // Initialization
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Initialize the download manager
     * @param settings Application settings
     * @return true on success
     */
    bool initialize(const Settings& settings);

    /**
     * @brief Set persistence manager
     */
    void setPersistenceManager(PersistenceManager* persistence);

    /**
     * @brief Set streaming resolver
     */
    void setStreamingResolver(StreamingResolver* resolver);

    /**
     * @brief Load saved downloads from database
     */
    void loadSavedDownloads();

    // ═══════════════════════════════════════════════════════════════════════════
    // Download Operations
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Add a new download
     * @param url URL to download
     * @param savePath Path to save the file
     * @param fileName Optional filename (auto-detected if empty)
     * @return Download ID (UUID)
     */
    Q_INVOKABLE QString addDownload(const QUrl& url,
                                     const QString& savePath = QString(),
                                     const QString& fileName = QString());

    /**
     * @brief Add download with full options
     * @param info Download information
     * @return Download ID
     */
    QString addDownload(const DownloadInfo& info);

    /**
     * @brief Start a specific download
     */
    Q_INVOKABLE void startDownload(const QString& id);

    /**
     * @brief Pause a specific download
     */
    Q_INVOKABLE void pauseDownload(const QString& id);

    /**
     * @brief Resume a paused download
     */
    Q_INVOKABLE void resumeDownload(const QString& id);

    /**
     * @brief Cancel a download
     * @param deleteFiles Also delete downloaded files
     */
    Q_INVOKABLE void cancelDownload(const QString& id, bool deleteFiles = true);

    /**
     * @brief Retry a failed download
     */
    Q_INVOKABLE void retryDownload(const QString& id);

    /**
     * @brief Remove a download from the list
     * @param deleteFiles Also delete downloaded files
     */
    Q_INVOKABLE void removeDownload(const QString& id, bool deleteFiles = false);

    /**
     * @brief Start all queued downloads
     */
    Q_INVOKABLE void startAll();

    /**
     * @brief Pause all active downloads
     */
    Q_INVOKABLE void pauseAll();

    /**
     * @brief Resume all paused downloads
     */
    Q_INVOKABLE void resumeAll();

    /**
     * @brief Cancel all downloads
     */
    Q_INVOKABLE void cancelAll(bool deleteFiles = false);

    /**
     * @brief Clear completed downloads from list
     */
    Q_INVOKABLE void clearCompleted();

    // ═══════════════════════════════════════════════════════════════════════════
    // Queries
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Get download by ID
     */
    [[nodiscard]] DownloadTask* download(const QString& id) const;

    /**
     * @brief Get download info by ID
     */
    [[nodiscard]] DownloadInfo downloadInfo(const QString& id) const;

    /**
     * @brief Get all download info
     */
    [[nodiscard]] std::vector<DownloadInfo> allDownloads() const;

    /**
     * @brief Get active downloads
     */
    [[nodiscard]] std::vector<DownloadInfo> activeDownloads() const;

    /**
     * @brief Get queued downloads
     */
    [[nodiscard]] std::vector<DownloadInfo> queuedDownloads() const;

    /**
     * @brief Get completed downloads
     */
    [[nodiscard]] std::vector<DownloadInfo> completedDownloads() const;

    // ═══════════════════════════════════════════════════════════════════════════
    // Properties
    // ═══════════════════════════════════════════════════════════════════════════

    [[nodiscard]] int activeCount() const;
    [[nodiscard]] int queuedCount() const;
    [[nodiscard]] int totalCount() const;
    [[nodiscard]] double totalSpeed() const;

    [[nodiscard]] int maxConcurrent() const { return m_maxConcurrent; }
    void setMaxConcurrent(int max);

    [[nodiscard]] qint64 globalSpeedLimit() const { return m_globalSpeedLimit; }
    void setGlobalSpeedLimit(qint64 bytesPerSecond);

    [[nodiscard]] QString defaultSavePath() const { return m_defaultSavePath; }
    void setDefaultSavePath(const QString& path);

    [[nodiscard]] int defaultMaxSegments() const { return m_defaultMaxSegments; }
    void setDefaultMaxSegments(int segments);

signals:
    /**
     * @brief New download added
     */
    void downloadAdded(const QString& id);

    /**
     * @brief Download removed
     */
    void downloadRemoved(const QString& id);

    /**
     * @brief Download state changed
     */
    void downloadStateChanged(const QString& id, DownloadState newState);

    /**
     * @brief Download progress updated
     */
    void downloadProgressUpdated(const QString& id, qint64 downloaded,
                                  qint64 total, double percent);

    /**
     * @brief Download completed
     */
    void downloadCompleted(const QString& id);

    /**
     * @brief Download error
     */
    void downloadError(const QString& id, const QString& message);

    /**
     * @brief Active count changed
     */
    void activeCountChanged(int count);

    /**
     * @brief Queued count changed
     */
    void queuedCountChanged(int count);

    /**
     * @brief Total count changed
     */
    void totalCountChanged(int count);

    /**
     * @brief Total speed updated
     */
    void totalSpeedUpdated(double bytesPerSecond);

    /**
     * @brief Max concurrent changed
     */
    void maxConcurrentChanged(int max);

private slots:
    void onDownloadStateChanged(DownloadState newState);
    void onDownloadProgress(qint64 downloaded, qint64 total, double percent);
    void onDownloadSpeed(double bytesPerSecond);
    void onDownloadCompleted();
    void onDownloadError(const QString& message);

private:
    explicit DownloadManager(QObject* parent = nullptr);

    void processQueue();
    void updateTotalSpeed();
    DownloadTask* createTask(const DownloadInfo& info);

    static DownloadManager* s_instance;

    mutable QMutex m_mutex;
    QHash<QString, std::unique_ptr<DownloadTask>> m_downloads;
    std::vector<QString> m_queue;  // IDs in queue order

    PersistenceManager* m_persistence = nullptr;
    StreamingResolver* m_streamingResolver = nullptr;

    std::unique_ptr<AggregateSpeedCalculator> m_speedCalculator;

    int m_maxConcurrent = Config::DEFAULT_MAX_CONCURRENT_DOWNLOADS;
    qint64 m_globalSpeedLimit = 0;
    QString m_defaultSavePath;
    int m_defaultMaxSegments = Config::DEFAULT_SEGMENTS;

    bool m_initialized = false;
};

} // namespace OpenIDM

#endif // OPENIDM_DOWNLOADMANAGER_H
