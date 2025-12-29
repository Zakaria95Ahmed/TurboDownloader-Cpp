/**
 * @file DownloadManager.h
 * @brief Central manager for all download operations
 * 
 * DownloadManager is the main entry point for the download engine.
 * It manages the lifecycle of all downloads, coordinates resources,
 * and provides the interface for the UI layer.
 */

#pragma once

#include "openidm/engine/Types.h"
#include "openidm/engine/DownloadTask.h"

#include <memory>
#include <vector>
#include <map>
#include <optional>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QSettings>

namespace OpenIDM {

// Forward declarations
class PersistenceManager;
class SettingsManager;

/**
 * @class DownloadManager
 * @brief Singleton manager for all download operations
 * 
 * Key responsibilities:
 * 1. Create and manage download tasks
 * 2. Queue management and scheduling
 * 3. Global bandwidth limiting
 * 4. Statistics aggregation
 * 5. Settings and configuration
 * 6. Integration with persistence layer
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Internal mutex protects task map
 * - Signals are emitted on the main thread
 */
class DownloadManager : public QObject {
    Q_OBJECT
    
    // Properties for QML
    Q_PROPERTY(int activeDownloads READ activeDownloadCount NOTIFY activeCountChanged)
    Q_PROPERTY(int queuedDownloads READ queuedDownloadCount NOTIFY queueCountChanged)
    Q_PROPERTY(int completedDownloads READ completedDownloadCount NOTIFY completedCountChanged)
    Q_PROPERTY(int totalDownloads READ totalDownloadCount NOTIFY totalCountChanged)
    Q_PROPERTY(double globalSpeed READ globalSpeed NOTIFY globalSpeedChanged)
    Q_PROPERTY(QString globalSpeedFormatted READ globalSpeedFormatted NOTIFY globalSpeedChanged)
    Q_PROPERTY(int maxConcurrentDownloads READ maxConcurrentDownloads WRITE setMaxConcurrentDownloads NOTIFY settingsChanged)
    Q_PROPERTY(QString defaultDownloadDirectory READ defaultDownloadDirectory WRITE setDefaultDownloadDirectory NOTIFY settingsChanged)

public:
    // ───────────────────────────────────────────────────────────────────────
    // Singleton Access
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Get the singleton instance
     * @return Reference to the download manager
     */
    static DownloadManager& instance();
    
    /**
     * @brief Initialize the manager
     * @param parent Optional parent object
     * @return True if initialization successful
     * 
     * Must be called once at application startup.
     * Loads persisted downloads and initializes subsystems.
     */
    static bool initialize(QObject* parent = nullptr);
    
    /**
     * @brief Shutdown the manager
     * 
     * Pauses all active downloads, saves state, and cleans up.
     */
    static void shutdown();
    
    // Destructor
    ~DownloadManager() override;
    
    // ───────────────────────────────────────────────────────────────────────
    // Download Management
    // ───────────────────────────────────────────────────────────────────────
    
public slots:
    /**
     * @brief Add a new download
     * @param url URL to download
     * @param destPath Destination path (file or directory)
     * @param startImmediately Whether to start right away
     * @return Task ID of the new download, or null if failed
     */
    TaskId addDownload(const QUrl& url, 
                       const QString& destPath = QString(),
                       bool startImmediately = true);
    
    /**
     * @brief Add a new download (string version for QML)
     */
    Q_INVOKABLE QString addDownloadUrl(const QString& url,
                                        const QString& destPath = QString(),
                                        bool startImmediately = true);
    
    /**
     * @brief Add multiple downloads at once
     * @param urls List of URLs
     * @param destDir Destination directory
     * @return List of created task IDs
     */
    std::vector<TaskId> addDownloads(const QList<QUrl>& urls,
                                      const QString& destDir = QString());
    
    /**
     * @brief Remove a download
     * @param id Task ID
     * @param deleteFile Also delete downloaded/partial file
     */
    Q_INVOKABLE void removeDownload(const QString& id, bool deleteFile = false);
    void removeDownload(const TaskId& id, bool deleteFile = false);
    
    /**
     * @brief Remove all downloads
     * @param deleteFiles Delete all files
     */
    Q_INVOKABLE void removeAllDownloads(bool deleteFiles = false);
    
    /**
     * @brief Remove completed downloads
     */
    Q_INVOKABLE void clearCompleted();
    
public:
    // ───────────────────────────────────────────────────────────────────────
    // Task Access
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Get a download task by ID
     * @param id Task ID
     * @return Task pointer or nullptr
     */
    DownloadTask* task(const TaskId& id) const;
    
    /**
     * @brief Get task by string ID (for QML)
     */
    Q_INVOKABLE DownloadTask* taskById(const QString& id) const;
    
    /**
     * @brief Get all tasks
     * @return Vector of all task pointers
     */
    std::vector<DownloadTask*> allTasks() const;
    
    /**
     * @brief Get tasks in a specific state
     * @param state State to filter by
     * @return Tasks matching state
     */
    std::vector<DownloadTask*> tasksInState(DownloadState state) const;
    
    /**
     * @brief Check if a URL is already being downloaded
     * @param url URL to check
     * @return Existing task ID if found
     */
    std::optional<TaskId> findByUrl(const QUrl& url) const;
    
    // ───────────────────────────────────────────────────────────────────────
    // Bulk Actions
    // ───────────────────────────────────────────────────────────────────────
    
public slots:
    /**
     * @brief Start a download
     * @param id Task ID
     */
    Q_INVOKABLE void startDownload(const QString& id);
    void startDownload(const TaskId& id);
    
    /**
     * @brief Pause a download
     * @param id Task ID
     */
    Q_INVOKABLE void pauseDownload(const QString& id);
    void pauseDownload(const TaskId& id);
    
    /**
     * @brief Resume a download
     * @param id Task ID
     */
    Q_INVOKABLE void resumeDownload(const QString& id);
    void resumeDownload(const TaskId& id);
    
    /**
     * @brief Cancel a download
     * @param id Task ID
     */
    Q_INVOKABLE void cancelDownload(const QString& id);
    void cancelDownload(const TaskId& id);
    
    /**
     * @brief Retry a failed download
     * @param id Task ID
     */
    Q_INVOKABLE void retryDownload(const QString& id);
    void retryDownload(const TaskId& id);
    
    /**
     * @brief Pause all active downloads
     */
    Q_INVOKABLE void pauseAll();
    
    /**
     * @brief Resume all paused downloads
     */
    Q_INVOKABLE void resumeAll();
    
    /**
     * @brief Start all queued downloads
     */
    Q_INVOKABLE void startAll();
    
public:
    // ───────────────────────────────────────────────────────────────────────
    // Statistics
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Number of active downloads
    int activeDownloadCount() const;
    
    /// @return Number of queued downloads
    int queuedDownloadCount() const;
    
    /// @return Number of completed downloads
    int completedDownloadCount() const;
    
    /// @return Total download count
    int totalDownloadCount() const;
    
    /// @return Global download speed (all active tasks)
    SpeedBps globalSpeed() const;
    
    /// @return Formatted global speed
    QString globalSpeedFormatted() const { return formatSpeed(globalSpeed()); }
    
    /// @return Total bytes downloaded (all time)
    ByteCount totalBytesDownloaded() const;
    
    /// @return Session bytes downloaded
    ByteCount sessionBytesDownloaded() const;
    
    // ───────────────────────────────────────────────────────────────────────
    // Settings
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Maximum concurrent downloads
    int maxConcurrentDownloads() const { return m_maxConcurrent; }
    
    /**
     * @brief Set maximum concurrent downloads
     * @param count Max count (1-16)
     */
    void setMaxConcurrentDownloads(int count);
    
    /// @return Default download directory
    QString defaultDownloadDirectory() const { return m_defaultDir; }
    
    /**
     * @brief Set default download directory
     * @param path Directory path
     */
    void setDefaultDownloadDirectory(const QString& path);
    
    /// @return Maximum segments per download
    int maxSegmentsPerDownload() const { return m_maxSegments; }
    
    /**
     * @brief Set maximum segments per download
     * @param count Max segments (1-32)
     */
    void setMaxSegmentsPerDownload(int count);
    
    /// @return Global speed limit (0 = unlimited)
    SpeedBps speedLimit() const { return m_speedLimit; }
    
    /**
     * @brief Set global speed limit
     * @param limit Bytes per second (0 = unlimited)
     */
    void setSpeedLimit(SpeedBps limit);
    
    // ───────────────────────────────────────────────────────────────────────
    // Persistence
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Get persistence manager
     * @return Pointer to persistence manager
     */
    PersistenceManager* persistence() const { return m_persistence.get(); }
    
    /**
     * @brief Force save all state
     */
    void saveState();
    
    /**
     * @brief Load state from database
     */
    void loadState();
    
signals:
    // ───────────────────────────────────────────────────────────────────────
    // Signals
    // ───────────────────────────────────────────────────────────────────────
    
    /// Emitted when a download is added
    void downloadAdded(const TaskId& id);
    
    /// Emitted when a download is removed
    void downloadRemoved(const TaskId& id);
    
    /// Emitted when a download starts
    void downloadStarted(const TaskId& id);
    
    /// Emitted when a download pauses
    void downloadPaused(const TaskId& id);
    
    /// Emitted when a download resumes
    void downloadResumed(const TaskId& id);
    
    /// Emitted when a download completes
    void downloadCompleted(const TaskId& id);
    
    /// Emitted when a download fails
    void downloadFailed(const TaskId& id, const QString& error);
    
    /// Emitted when active count changes
    void activeCountChanged();
    
    /// Emitted when queue count changes
    void queueCountChanged();
    
    /// Emitted when completed count changes
    void completedCountChanged();
    
    /// Emitted when total count changes
    void totalCountChanged();
    
    /// Emitted when global speed changes
    void globalSpeedChanged();
    
    /// Emitted when settings change
    void settingsChanged();
    
private slots:
    void onTaskStateChanged(DownloadState newState);
    void onTaskCompleted();
    void onTaskFailed(const DownloadError& error);
    void onTaskNeedsPersistence();
    void onSpeedUpdateTimer();
    void processQueue();
    
private:
    // Private constructor (singleton)
    explicit DownloadManager(QObject* parent = nullptr);
    
    // Internal helpers
    void connectTask(DownloadTask* task);
    void disconnectTask(DownloadTask* task);
    void updateCounts();
    bool canStartMore() const;
    void startNextQueued();
    
    // Singleton instance
    static std::unique_ptr<DownloadManager> s_instance;
    static bool s_initialized;
    
    // Task storage
    std::map<TaskId, std::unique_ptr<DownloadTask>> m_tasks;
    mutable std::mutex m_tasksMutex;
    
    // Persistence
    std::unique_ptr<PersistenceManager> m_persistence;
    
    // Settings
    int m_maxConcurrent{Constants::DEFAULT_CONCURRENT_DOWNLOADS};
    int m_maxSegments{Constants::DEFAULT_SEGMENTS};
    QString m_defaultDir;
    SpeedBps m_speedLimit{0};
    
    // Statistics
    std::atomic<ByteCount> m_totalBytesEver{0};
    std::atomic<ByteCount> m_sessionBytes{0};
    std::atomic<SpeedBps> m_globalSpeed{0.0};
    
    // Counts (for efficient property access)
    std::atomic<int> m_activeCount{0};
    std::atomic<int> m_queuedCount{0};
    std::atomic<int> m_completedCount{0};
    
    // Timers
    QTimer* m_speedTimer{nullptr};
    QTimer* m_queueTimer{nullptr};
};

} // namespace OpenIDM
