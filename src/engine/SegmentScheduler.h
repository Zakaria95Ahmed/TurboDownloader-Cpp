/**
 * @file SegmentScheduler.h
 * @brief Dynamic segment scheduler with work-stealing algorithm
 *
 * The SegmentScheduler is the brain of the multi-segment download system.
 * It manages segment workers, implements dynamic re-segmentation, and
 * applies work-stealing when workers finish early.
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_SEGMENTSCHEDULER_H
#define OPENIDM_SEGMENTSCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QReadWriteLock>

#include <memory>
#include <vector>
#include <queue>

#include "DownloadTypes.h"
#include "SegmentWorker.h"
#include "SpeedCalculator.h"

namespace OpenIDM {

/**
 * @brief Result of initial segment planning
 */
struct SegmentPlan {
    bool success = false;
    bool supportsRanges = false;
    qint64 totalSize = -1;
    int segmentCount = 1;
    std::vector<SegmentInfo> segments;
    QString errorMessage;
};

/**
 * @brief Manages dynamic segmentation and work distribution
 *
 * Key responsibilities:
 * - Create initial segment plan based on server capabilities
 * - Spawn and manage SegmentWorker instances
 * - Implement work-stealing algorithm
 * - Aggregate progress from all segments
 * - Handle segment completion and file merging
 */
class SegmentScheduler : public QObject {
    Q_OBJECT

    Q_PROPERTY(int activeWorkers READ activeWorkers NOTIFY workersChanged)
    Q_PROPERTY(int totalSegments READ totalSegments NOTIFY workersChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressUpdated)
    Q_PROPERTY(double speed READ speed NOTIFY speedUpdated)

public:
    /**
     * @brief Constructor
     * @param downloadId Parent download's UUID
     * @param parent QObject parent
     */
    explicit SegmentScheduler(const QString& downloadId, QObject* parent = nullptr);
    ~SegmentScheduler() override;

    // Non-copyable
    SegmentScheduler(const SegmentScheduler&) = delete;
    SegmentScheduler& operator=(const SegmentScheduler&) = delete;

    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Set maximum number of segments
     */
    void setMaxSegments(int max);

    /**
     * @brief Set speed limit per segment (bytes/sec)
     */
    void setSpeedLimitPerSegment(qint64 bytesPerSecond);

    /**
     * @brief Set save path for temporary files
     */
    void setSavePath(const QString& path);

    /**
     * @brief Set custom HTTP headers
     */
    void setHeaders(const QStringList& headers);

    // ═══════════════════════════════════════════════════════════════════════════
    // Planning & Execution
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Plan segments for a URL
     *
     * Performs HEAD request to determine:
     * - File size
     * - Range support
     * - Optimal segment count
     *
     * @param url Download URL
     * @return Segment plan with proposed segments
     */
    [[nodiscard]] SegmentPlan planSegments(const QUrl& url);

    /**
     * @brief Initialize scheduler with existing segment info
     *
     * Used for resuming downloads.
     *
     * @param url Download URL
     * @param segments Existing segment information
     * @param totalSize Total file size
     * @param supportsRanges Whether server supports ranges
     */
    void initialize(const QUrl& url, const std::vector<SegmentInfo>& segments,
                    qint64 totalSize, bool supportsRanges);

    // ═══════════════════════════════════════════════════════════════════════════
    // Control
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Start all segment workers
     */
    void start();

    /**
     * @brief Pause all workers
     */
    void pause();

    /**
     * @brief Resume all workers
     */
    void resume();

    /**
     * @brief Stop all workers
     */
    void stop();

    // ═══════════════════════════════════════════════════════════════════════════
    // Status
    // ═══════════════════════════════════════════════════════════════════════════

    [[nodiscard]] int activeWorkers() const;
    [[nodiscard]] int totalSegments() const;
    [[nodiscard]] int completedSegments() const;
    [[nodiscard]] qint64 totalBytesDownloaded() const;
    [[nodiscard]] double progress() const;
    [[nodiscard]] double speed() const;
    [[nodiscard]] qint64 eta() const;

    /**
     * @brief Get all segment information
     */
    [[nodiscard]] std::vector<SegmentInfo> segments() const;

    /**
     * @brief Get aggregated statistics
     */
    [[nodiscard]] DownloadStats stats() const;

    /**
     * @brief Check if all segments are complete
     */
    [[nodiscard]] bool isComplete() const;

    /**
     * @brief Check if download is paused
     */
    [[nodiscard]] bool isPaused() const { return m_paused; }

signals:
    /**
     * @brief Emitted when worker count changes
     */
    void workersChanged(int active, int total);

    /**
     * @brief Emitted with progress updates
     */
    void progressUpdated(qint64 downloaded, qint64 total, double percent);

    /**
     * @brief Emitted when speed is recalculated
     */
    void speedUpdated(double bytesPerSecond);

    /**
     * @brief Emitted when all segments complete
     */
    void allSegmentsCompleted();

    /**
     * @brief Emitted on segment error
     */
    void segmentError(int segmentId, const QString& message);

    /**
     * @brief Emitted when segment states change (for persistence)
     */
    void segmentStatesChanged(const std::vector<SegmentInfo>& segments);

private slots:
    void onWorkerCompleted(int segmentId);
    void onWorkerError(int segmentId, const QString& message);
    void onWorkerPaused(int segmentId);
    void onWorkerProgress(int segmentId, qint64 downloaded, qint64 total);
    void onWorkerBytesReceived(int segmentId, qint64 bytes);
    void onWorkerSpeedUpdated(int segmentId, double speed);
    void onSchedulerTick();

private:
    void createWorker(const SegmentInfo& segment);
    void performWorkStealing();
    void updateProgress();
    int findLargestSegment() const;
    QString partFilePath(int segmentId) const;

    QString m_downloadId;
    QUrl m_url;
    QString m_savePath;
    QStringList m_headers;

    qint64 m_totalSize = -1;
    bool m_supportsRanges = false;
    int m_maxSegments = Config::DEFAULT_SEGMENTS;
    qint64 m_speedLimitPerSegment = 0;

    bool m_started = false;
    bool m_paused = false;
    bool m_stopped = false;

    // Worker management
    mutable QReadWriteLock m_workersLock;
    std::vector<std::unique_ptr<SegmentWorkerThread>> m_workers;
    std::vector<SegmentInfo> m_segmentInfo;
    int m_nextSegmentId = 0;

    // Progress tracking
    std::atomic<qint64> m_totalBytesDownloaded{0};
    std::unique_ptr<SpeedCalculator> m_aggregateSpeed;

    // Scheduler timer for work-stealing
    QTimer* m_schedulerTimer = nullptr;

    // Completed segments
    std::atomic<int> m_completedCount{0};
};

} // namespace OpenIDM

#endif // OPENIDM_SEGMENTSCHEDULER_H
