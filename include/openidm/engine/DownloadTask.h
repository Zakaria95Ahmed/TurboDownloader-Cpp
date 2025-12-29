/**
 * @file DownloadTask.h
 * @brief Represents a single file download with multi-segment support
 * 
 * DownloadTask coordinates the complete lifecycle of downloading a single file:
 * - Server capability probing
 * - Segment initialization and scheduling
 * - Worker thread management
 * - Progress aggregation
 * - File merging upon completion
 */

#pragma once

#include "openidm/engine/Types.h"
#include "openidm/engine/Segment.h"
#include "openidm/engine/SegmentScheduler.h"
#include "openidm/engine/SegmentWorker.h"

#include <memory>
#include <vector>
#include <atomic>
#include <chrono>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QThreadPool>
#include <QTimer>

namespace OpenIDM {

// Forward declarations
class PersistenceManager;
class NetworkProbe;

/**
 * @class DownloadTask
 * @brief Manages the download of a single file
 * 
 * Responsibilities:
 * 1. Probe server to detect capabilities (range support, file size)
 * 2. Create and manage segment workers
 * 3. Coordinate segment scheduler for work distribution
 * 4. Track aggregate progress and speed
 * 5. Merge segment files upon completion
 * 6. Handle errors and retries at the task level
 * 
 * Thread Safety:
 * - Most state accessed from main thread
 * - Progress updates from worker threads via signals (queued)
 * - Atomic counters for statistics
 */
class DownloadTask : public QObject {
    Q_OBJECT
    
    // Properties for QML binding
    Q_PROPERTY(QString id READ idString CONSTANT)
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY totalSizeChanged)
    Q_PROPERTY(qint64 downloadedSize READ downloadedSize NOTIFY progressChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(QString speedFormatted READ speedFormatted NOTIFY speedChanged)
    Q_PROPERTY(QString remainingTime READ remainingTimeFormatted NOTIFY progressChanged)
    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)
    Q_PROPERTY(QString stateString READ stateString NOTIFY stateChanged)
    Q_PROPERTY(int activeSegments READ activeSegments NOTIFY progressChanged)
    Q_PROPERTY(int totalSegments READ totalSegments NOTIFY progressChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

public:
    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Create a new download task
     * @param url URL to download
     * @param destPath Destination file path (or directory)
     * @param parent QObject parent
     */
    DownloadTask(const QUrl& url, const QString& destPath, QObject* parent = nullptr);
    
    /**
     * @brief Create a task from persisted data
     * @param id Task ID
     * @param url URL
     * @param destPath Destination path
     * @param parent QObject parent
     */
    DownloadTask(const TaskId& id, const QUrl& url, const QString& destPath, QObject* parent = nullptr);
    
    ~DownloadTask() override;
    
    // Disable copying
    DownloadTask(const DownloadTask&) = delete;
    DownloadTask& operator=(const DownloadTask&) = delete;
    
    // ───────────────────────────────────────────────────────────────────────
    // Identification
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Unique task identifier
    TaskId id() const { return m_id; }
    
    /// @return Task ID as string (for QML)
    QString idString() const { return m_id.toString(QUuid::WithoutBraces); }
    
    /// @return Original download URL
    QString url() const { return m_url.toString(); }
    
    /// @return URL object
    QUrl urlObject() const { return m_url; }
    
    // ───────────────────────────────────────────────────────────────────────
    // File Information
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return File name (extracted from URL or Content-Disposition)
    QString fileName() const { return m_fileName; }
    
    /// @return Full destination path
    QString filePath() const { return m_filePath; }
    
    /// @return Directory where file will be saved
    QString directory() const;
    
    /// @return Content type (MIME)
    QString contentType() const { return m_capabilities.contentType; }
    
    /// @return Total file size in bytes (-1 if unknown)
    ByteCount totalSize() const { return m_totalSize.load(std::memory_order_relaxed); }
    
    /// @return True if file size is known
    bool isSizeKnown() const { return totalSize() > 0; }
    
    // ───────────────────────────────────────────────────────────────────────
    // State
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Current download state
    DownloadState state() const { return m_state.load(std::memory_order_acquire); }
    
    /// @return State as integer (for QML)
    int stateInt() const { return static_cast<int>(state()); }
    
    /// @return State as human-readable string
    QString stateString() const { return downloadStateToString(state()); }
    
    /// @return True if download is active
    bool isActive() const { return state() == DownloadState::Downloading; }
    
    /// @return True if download can be resumed
    bool isResumable() const { return state() == DownloadState::Paused; }
    
    /// @return True if download is finished (success or failure)
    bool isFinished() const { 
        auto s = state();
        return s == DownloadState::Completed || s == DownloadState::Failed;
    }
    
    // ───────────────────────────────────────────────────────────────────────
    // Progress
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Total bytes downloaded
    ByteCount downloadedSize() const { return m_downloadedBytes.load(std::memory_order_relaxed); }
    
    /// @return Progress as percentage (0-100)
    double progress() const;
    
    /// @return Current download speed in bytes/second
    SpeedBps speed() const { return m_currentSpeed.load(std::memory_order_relaxed); }
    
    /// @return Speed formatted for display
    QString speedFormatted() const { return formatSpeed(speed()); }
    
    /// @return Average speed since start
    SpeedBps averageSpeed() const;
    
    /// @return Estimated time remaining
    Duration remainingTime() const;
    
    /// @return Remaining time formatted for display
    QString remainingTimeFormatted() const { return formatDuration(remainingTime()); }
    
    /// @return Number of active segment workers
    int activeSegments() const;
    
    /// @return Total number of segments
    int totalSegments() const;
    
    /// @return Completed segments count
    int completedSegments() const;
    
    /// @return Detailed progress information
    DownloadProgress progressInfo() const;
    
    // ───────────────────────────────────────────────────────────────────────
    // Error Information
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Last error (if any)
    DownloadError lastError() const { return m_lastError; }
    
    /// @return Error message for display
    QString errorMessage() const { return m_lastError.message; }
    
    /// @return True if task has an error
    bool hasError() const { return m_lastError.hasError(); }
    
    // ───────────────────────────────────────────────────────────────────────
    // Server Capabilities
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Server capabilities from probe
    ServerCapabilities capabilities() const { return m_capabilities; }
    
    /// @return True if server supports range requests
    bool supportsRanges() const { return m_capabilities.supportsRanges; }
    
    // ───────────────────────────────────────────────────────────────────────
    // Priority
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Task priority
    Priority priority() const { return m_priority; }
    
    /// @brief Set task priority
    void setPriority(Priority priority);
    
    // ───────────────────────────────────────────────────────────────────────
    // Scheduler Access
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Segment scheduler (for workers)
    SegmentScheduler* scheduler() const { return m_scheduler.get(); }
    
    // ───────────────────────────────────────────────────────────────────────
    // Actions
    // ───────────────────────────────────────────────────────────────────────
    
public slots:
    /**
     * @brief Start or resume the download
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
     * @brief Cancel and remove the download
     */
    void cancel();
    
    /**
     * @brief Retry a failed download
     */
    void retry();
    
signals:
    // ───────────────────────────────────────────────────────────────────────
    // Signals
    // ───────────────────────────────────────────────────────────────────────
    
    /// Emitted when state changes
    void stateChanged(DownloadState newState);
    
    /// Emitted periodically during download
    void progressChanged();
    
    /// Emitted when speed changes
    void speedChanged();
    
    /// Emitted when file info is updated (after probe)
    void fileNameChanged();
    void filePathChanged();
    void totalSizeChanged();
    
    /// Emitted on error
    void errorChanged();
    void errorOccurred(const DownloadError& error);
    
    /// Emitted when download completes successfully
    void completed();
    
    /// Emitted when download fails
    void failed(const DownloadError& error);
    
    /// Emitted when task needs persistence update
    void needsPersistence();
    
private slots:
    // ───────────────────────────────────────────────────────────────────────
    // Internal Slots
    // ───────────────────────────────────────────────────────────────────────
    
    void onProbeCompleted(const ServerCapabilities& caps);
    void onProbeFailed(const DownloadError& error);
    void onSegmentCompleted(SegmentId id);
    void onSegmentFailed(SegmentId id, const QString& error);
    void onAllSegmentsCompleted();
    void onProgressTimer();
    
private:
    // ───────────────────────────────────────────────────────────────────────
    // Internal Methods
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Probe server capabilities
     */
    void probeServer();
    
    /**
     * @brief Initialize segments based on server capabilities
     */
    void initializeSegments();
    
    /**
     * @brief Start worker threads
     */
    void startWorkers();
    
    /**
     * @brief Stop all workers
     */
    void stopWorkers();
    
    /**
     * @brief Merge segment files into final file
     */
    bool mergeSegments();
    
    /**
     * @brief Verify final file integrity
     */
    bool verifyFile();
    
    /**
     * @brief Clean up temporary files
     */
    void cleanupTempFiles();
    
    /**
     * @brief Set download state
     * @param newState New state
     */
    void setState(DownloadState newState);
    
    /**
     * @brief Set error state
     * @param error Error information
     */
    void setError(const DownloadError& error);
    
    /**
     * @brief Update aggregate statistics
     */
    void updateStatistics();
    
    /**
     * @brief Generate temporary file path for a segment
     */
    QString tempFilePath(SegmentId segmentId) const;
    
    // ───────────────────────────────────────────────────────────────────────
    // Member Variables
    // ───────────────────────────────────────────────────────────────────────
    
    // Identification
    TaskId m_id;
    QUrl m_url;
    QString m_fileName;
    QString m_filePath;
    QString m_destDir;
    
    // Server info
    ServerCapabilities m_capabilities;
    
    // State
    std::atomic<DownloadState> m_state{DownloadState::Queued};
    std::atomic<ByteCount> m_totalSize{-1};
    std::atomic<ByteCount> m_downloadedBytes{0};
    std::atomic<SpeedBps> m_currentSpeed{0.0};
    DownloadError m_lastError;
    Priority m_priority{Priority::Normal};
    
    // Timing
    Timestamp m_startTime;
    Timestamp m_endTime;
    Duration m_elapsedTime{0};
    
    // Components
    std::unique_ptr<SegmentScheduler> m_scheduler;
    std::unique_ptr<NetworkProbe> m_probe;
    std::vector<std::unique_ptr<SegmentWorker>> m_workers;
    QThreadPool* m_threadPool;
    
    // Progress timer
    QTimer* m_progressTimer{nullptr};
    
    // Speed calculation
    std::vector<std::pair<Timestamp, ByteCount>> m_speedHistory;
    
    // Persistence
    bool m_needsPersistence{false};
    ByteCount m_lastPersistedBytes{0};
};

} // namespace OpenIDM
