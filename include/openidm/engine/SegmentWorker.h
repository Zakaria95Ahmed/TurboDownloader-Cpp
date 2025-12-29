/**
 * @file SegmentWorker.h
 * @brief Worker thread for downloading individual segments using libcurl
 * 
 * SegmentWorker is responsible for the actual HTTP transfer of a single segment.
 * It uses libcurl for fine-grained control over the connection and supports
 * pause/resume, progress reporting, and error handling.
 */

#pragma once

#include "openidm/engine/Types.h"
#include "openidm/engine/Segment.h"

#include <atomic>
#include <memory>
#include <chrono>

#include <QObject>
#include <QRunnable>
#include <QFile>
#include <QMutex>
#include <QWaitCondition>

// Forward declare CURL types
typedef void CURL;

namespace OpenIDM {

// Forward declarations
class SegmentScheduler;
class DownloadTask;

/**
 * @class SegmentWorker
 * @brief Downloads a segment using HTTP byte-range requests
 * 
 * Lifecycle:
 * 1. Worker is created and added to thread pool
 * 2. Worker acquires a segment from the scheduler
 * 3. Worker downloads the segment using libcurl
 * 4. On completion/error, worker returns segment to scheduler
 * 5. Worker attempts to acquire more work (work-stealing)
 * 6. If no work available, worker waits or terminates
 * 
 * Thread Safety:
 * - Worker runs in its own thread from QThreadPool
 * - Communicates with scheduler via thread-safe methods
 * - Uses atomic flags for stop/pause control
 * 
 * Memory Safety:
 * - Worker does not own the Segment (scheduler owns it)
 * - CURL handle is owned by the worker
 */
class SegmentWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    // ───────────────────────────────────────────────────────────────────────
    // Types
    // ───────────────────────────────────────────────────────────────────────
    
    /// Worker states
    enum class State {
        Idle,           ///< Not currently downloading
        Downloading,    ///< Actively downloading a segment
        Paused,         ///< Paused by user
        Stopping,       ///< Shutting down
        Error           ///< Encountered an error
    };
    
    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Construct a segment worker
     * @param task Parent download task
     * @param scheduler Segment scheduler for work acquisition
     * @param parent QObject parent
     */
    SegmentWorker(DownloadTask* task, SegmentScheduler* scheduler, QObject* parent = nullptr);
    
    ~SegmentWorker() override;
    
    // Disable copying
    SegmentWorker(const SegmentWorker&) = delete;
    SegmentWorker& operator=(const SegmentWorker&) = delete;
    
    // ───────────────────────────────────────────────────────────────────────
    // QRunnable Interface
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Main worker loop - called by QThreadPool
     * 
     * This method:
     * 1. Loops until stopped
     * 2. Acquires segments from scheduler
     * 3. Downloads using libcurl
     * 4. Reports progress and handles errors
     */
    void run() override;
    
    // ───────────────────────────────────────────────────────────────────────
    // Control
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Request the worker to stop
     * 
     * Sets stop flag and interrupts any ongoing transfer.
     * Worker will exit its run() loop on next iteration.
     */
    void stop();
    
    /**
     * @brief Pause the current download
     * 
     * Pauses libcurl transfer. Can be resumed with resume().
     */
    void pause();
    
    /**
     * @brief Resume a paused download
     */
    void resume();
    
    /**
     * @brief Check if worker should continue
     */
    bool shouldContinue() const { return !m_shouldStop.load(std::memory_order_acquire); }
    
    /**
     * @brief Check if worker is paused
     */
    bool isPaused() const { return m_isPaused.load(std::memory_order_acquire); }
    
    // ───────────────────────────────────────────────────────────────────────
    // State
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Current worker state
    State state() const { return m_state.load(std::memory_order_acquire); }
    
    /// @return Currently assigned segment (may be nullptr)
    Segment* currentSegment() const;
    
    /// @return True if worker is actively downloading
    bool isActive() const { return state() == State::Downloading; }
    
    // ───────────────────────────────────────────────────────────────────────
    // Statistics
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Current download speed (bytes/second)
    SpeedBps currentSpeed() const;
    
    /// @return Total bytes downloaded by this worker
    ByteCount totalBytesDownloaded() const { return m_totalBytesDownloaded.load(); }
    
    /// @return Bytes downloaded in current segment
    ByteCount segmentBytesDownloaded() const { return m_segmentBytesDownloaded.load(); }
    
signals:
    /// Emitted periodically during download
    void progressUpdated(ByteCount downloaded, SpeedBps speed);
    
    /// Emitted when segment download completes
    void segmentCompleted(Segment* segment);
    
    /// Emitted on error
    void errorOccurred(Segment* segment, const DownloadError& error);
    
    /// Emitted when worker state changes
    void stateChanged(State newState);
    
    /// Emitted when worker finishes (regardless of reason)
    void finished();
    
private:
    // ───────────────────────────────────────────────────────────────────────
    // Internal Methods
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Download a single segment
     * @param segment Segment to download
     * @return True on success, false on error
     */
    bool downloadSegment(Segment* segment);
    
    /**
     * @brief Initialize libcurl handle
     * @return True on success
     */
    bool initCurl();
    
    /**
     * @brief Configure curl for the current segment
     * @param segment Target segment
     * @return True on success
     */
    bool configureCurl(Segment* segment);
    
    /**
     * @brief Cleanup curl resources
     */
    void cleanupCurl();
    
    /**
     * @brief Open temporary file for segment
     * @param segment Target segment
     * @return True on success
     */
    bool openTempFile(Segment* segment);
    
    /**
     * @brief Close and flush temporary file
     */
    void closeTempFile();
    
    /**
     * @brief Handle curl error
     * @param code Curl result code
     * @param segment Affected segment
     * @return Populated error structure
     */
    DownloadError handleCurlError(int code, Segment* segment);
    
    /**
     * @brief Update speed calculation
     * @param bytes Bytes received
     */
    void updateSpeed(ByteCount bytes);
    
    /**
     * @brief Wait while paused
     */
    void waitWhilePaused();
    
    // ───────────────────────────────────────────────────────────────────────
    // Curl Callbacks (static)
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Curl write callback - receives data
     */
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    
    /**
     * @brief Curl progress callback - for pause/cancel
     */
    static int progressCallback(void* clientp, 
                                 double dltotal, double dlnow,
                                 double ultotal, double ulnow);
    
    /**
     * @brief Curl header callback - for response headers
     */
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    
    // ───────────────────────────────────────────────────────────────────────
    // Member Variables
    // ───────────────────────────────────────────────────────────────────────
    
    // Parent references (not owned)
    DownloadTask* m_task;
    SegmentScheduler* m_scheduler;
    
    // Current segment (owned by scheduler)
    Segment* m_currentSegment{nullptr};
    mutable QMutex m_segmentMutex;
    
    // Curl handle
    CURL* m_curl{nullptr};
    
    // Temp file
    std::unique_ptr<QFile> m_tempFile;
    
    // Control flags
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_isPaused{false};
    std::atomic<State> m_state{State::Idle};
    
    // Pause synchronization
    QMutex m_pauseMutex;
    QWaitCondition m_pauseCondition;
    
    // Statistics
    std::atomic<ByteCount> m_totalBytesDownloaded{0};
    std::atomic<ByteCount> m_segmentBytesDownloaded{0};
    
    // Speed calculation
    struct SpeedSample {
        ByteCount bytes;
        Timestamp time;
    };
    std::vector<SpeedSample> m_speedSamples;
    mutable QMutex m_speedMutex;
    std::atomic<SpeedBps> m_currentSpeed{0.0};
    
    // Timing
    Timestamp m_segmentStartTime;
    Timestamp m_lastProgressReport;
};

} // namespace OpenIDM
