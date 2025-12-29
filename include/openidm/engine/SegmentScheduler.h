/**
 * @file SegmentScheduler.h
 * @brief Work-stealing segment scheduler for dynamic load balancing
 * 
 * The SegmentScheduler is the heart of OpenIDM's parallel download strategy.
 * It implements a work-stealing algorithm that dynamically rebalances segments
 * across worker threads to maximize bandwidth utilization.
 */

#pragma once

#include "openidm/engine/Types.h"
#include "openidm/engine/Segment.h"

#include <memory>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <shared_mutex>
#include <condition_variable>
#include <functional>

#include <QObject>
#include <QTimer>

namespace OpenIDM {

// Forward declarations
class SegmentWorker;
class DownloadTask;

/**
 * @class SegmentScheduler
 * @brief Manages segment distribution and work-stealing among worker threads
 * 
 * Key responsibilities:
 * 1. Initial segment allocation based on file size and connection count
 * 2. Work-stealing when workers finish early
 * 3. Dynamic re-segmentation of slow/large segments
 * 4. Throughput monitoring and rebalancing
 * 5. Thread synchronization for segment state changes
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses reader-writer lock for read-heavy segment queries
 * - Condition variable for worker wait/notify
 */
class SegmentScheduler : public QObject {
    Q_OBJECT

public:
    // ───────────────────────────────────────────────────────────────────────
    // Types
    // ───────────────────────────────────────────────────────────────────────
    
    /// Callback for segment completion
    using SegmentCompleteCallback = std::function<void(Segment*)>;
    
    /// Worker throughput entry
    struct WorkerStats {
        SegmentWorker* worker;
        Segment* segment;
        SpeedBps throughput;
        ByteCount bytesDownloaded;
        Timestamp lastUpdate;
    };
    
    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Construct scheduler
     * @param task Parent download task
     * @param parent QObject parent
     */
    explicit SegmentScheduler(DownloadTask* task, QObject* parent = nullptr);
    
    ~SegmentScheduler() override;
    
    // Disable copying
    SegmentScheduler(const SegmentScheduler&) = delete;
    SegmentScheduler& operator=(const SegmentScheduler&) = delete;
    
    // ───────────────────────────────────────────────────────────────────────
    // Initialization
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Initialize segments for a download
     * @param totalSize Total file size in bytes
     * @param segmentCount Number of segments to create
     * @return Vector of created segments
     */
    std::vector<Segment*> initializeSegments(ByteCount totalSize, size_t segmentCount);
    
    /**
     * @brief Restore segments from persistence
     * @param snapshots Segment snapshots from database
     */
    void restoreSegments(const std::vector<Segment::Snapshot>& snapshots);
    
    /**
     * @brief Calculate optimal segment count based on file size
     * @param totalSize File size in bytes
     * @return Recommended number of segments
     */
    static size_t calculateOptimalSegmentCount(ByteCount totalSize);
    
    // ───────────────────────────────────────────────────────────────────────
    // Segment Access
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Get all segments (read-only view)
     * @return Vector of segment pointers
     */
    std::vector<Segment*> allSegments() const;
    
    /**
     * @brief Get segment by ID
     * @param id Segment ID
     * @return Segment pointer or nullptr
     */
    Segment* segment(SegmentId id) const;
    
    /**
     * @brief Get number of segments
     */
    size_t segmentCount() const;
    
    /**
     * @brief Get segments in a specific state
     * @param state State to filter by
     * @return Segments matching state
     */
    std::vector<Segment*> segmentsInState(SegmentState state) const;
    
    // ───────────────────────────────────────────────────────────────────────
    // Work Distribution
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Get next pending segment for a worker
     * @param worker Requesting worker
     * @return Segment to download or nullptr if none available
     * 
     * This method implements the work-stealing algorithm:
     * 1. First, check pending queue for unassigned segments
     * 2. If empty, attempt to steal from the largest active segment
     * 3. If no work available, return nullptr (worker should wait)
     */
    Segment* acquireSegment(SegmentWorker* worker);
    
    /**
     * @brief Return a segment (completed or paused)
     * @param worker Worker returning the segment
     * @param segment Segment being returned
     */
    void releaseSegment(SegmentWorker* worker, Segment* segment);
    
    /**
     * @brief Attempt to steal work from another segment
     * @param worker Worker seeking work
     * @return New segment split from largest remaining, or nullptr
     */
    Segment* stealWork(SegmentWorker* worker);
    
    /**
     * @brief Mark segment as completed
     * @param segment Completed segment
     */
    void markCompleted(Segment* segment);
    
    /**
     * @brief Mark segment as failed
     * @param segment Failed segment
     * @param error Error information
     */
    void markFailed(Segment* segment, const QString& error);
    
    // ───────────────────────────────────────────────────────────────────────
    // Worker Management
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Register a worker with the scheduler
     * @param worker Worker to register
     */
    void registerWorker(SegmentWorker* worker);
    
    /**
     * @brief Unregister a worker
     * @param worker Worker to remove
     */
    void unregisterWorker(SegmentWorker* worker);
    
    /**
     * @brief Get number of active workers
     */
    size_t activeWorkerCount() const;
    
    /**
     * @brief Notify that a worker is idle and waiting for work
     * @param worker Idle worker
     */
    void notifyWorkerIdle(SegmentWorker* worker);
    
    /**
     * @brief Wake all idle workers (e.g., after adding new segments)
     */
    void wakeAllWorkers();
    
    /**
     * @brief Wait for work availability (called by workers)
     * @param timeout Maximum wait time
     * @return True if work may be available, false if timeout
     */
    bool waitForWork(Duration timeout);
    
    // ───────────────────────────────────────────────────────────────────────
    // Throughput Monitoring
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Report throughput for a worker's segment
     * @param worker Reporting worker
     * @param bytesPerSecond Current throughput
     */
    void reportThroughput(SegmentWorker* worker, SpeedBps bytesPerSecond);
    
    /**
     * @brief Get aggregate download speed
     * @return Total bytes per second across all workers
     */
    SpeedBps totalThroughput() const;
    
    /**
     * @brief Get throughput statistics for all workers
     * @return Map of worker to stats
     */
    std::vector<WorkerStats> workerStats() const;
    
    // ───────────────────────────────────────────────────────────────────────
    // Rebalancing
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Trigger rebalancing of segments based on throughput
     * 
     * Called periodically to:
     * 1. Identify slow segments
     * 2. Split large remaining segments
     * 3. Reassign work to faster workers
     */
    void rebalanceSegments();
    
    /**
     * @brief Enable/disable automatic rebalancing
     * @param enabled Whether to auto-rebalance
     * @param interval Rebalance check interval
     */
    void setAutoRebalance(bool enabled, Duration interval = Constants::REBALANCE_INTERVAL);
    
    // ───────────────────────────────────────────────────────────────────────
    // Progress
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Calculate aggregate progress
     * @return Total downloaded bytes across all segments
     */
    ByteCount totalDownloadedBytes() const;
    
    /**
     * @brief Check if all segments are complete
     */
    bool isAllComplete() const;
    
    /**
     * @brief Check if download has failed (unrecoverable)
     */
    bool hasFailed() const;
    
    // ───────────────────────────────────────────────────────────────────────
    // Control
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Pause all active segments
     */
    void pauseAll();
    
    /**
     * @brief Resume paused segments
     */
    void resumeAll();
    
    /**
     * @brief Cancel all segments
     */
    void cancelAll();
    
    /**
     * @brief Reset scheduler state
     */
    void reset();
    
signals:
    /// Emitted when a segment completes
    void segmentCompleted(SegmentId id);
    
    /// Emitted when a segment fails
    void segmentFailed(SegmentId id, const QString& error);
    
    /// Emitted when all segments complete
    void allSegmentsCompleted();
    
    /// Emitted when a new segment is created (split)
    void segmentAdded(SegmentId id);
    
    /// Emitted periodically with progress update
    void progressUpdated(ByteCount downloaded, ByteCount total);
    
    /// Emitted when rebalancing occurs
    void rebalanced(int splitCount);
    
private slots:
    void onRebalanceTimer();
    
private:
    // Internal helpers
    Segment* findLargestActiveSegment() const;
    Segment* createNewSegment(ByteOffset start, ByteOffset end);
    void scheduleSegment(Segment* segment);
    SegmentId nextSegmentId();
    
    // Parent task
    DownloadTask* m_task;
    
    // Segment storage
    std::vector<std::unique_ptr<Segment>> m_segments;
    std::deque<Segment*> m_pendingQueue;
    std::set<Segment*> m_activeSegments;
    std::set<Segment*> m_completedSegments;
    std::set<Segment*> m_failedSegments;
    
    // Worker tracking
    std::set<SegmentWorker*> m_workers;
    std::map<SegmentWorker*, Segment*> m_workerAssignments;
    std::map<SegmentWorker*, WorkerStats> m_workerStats;
    
    // Synchronization
    mutable std::shared_mutex m_mutex;
    std::condition_variable_any m_workCondition;
    
    // Rebalancing
    QTimer* m_rebalanceTimer{nullptr};
    bool m_autoRebalance{true};
    
    // ID generation
    std::atomic<SegmentId> m_nextSegmentId{0};
    
    // State
    bool m_paused{false};
    bool m_cancelled{false};
};

} // namespace OpenIDM
