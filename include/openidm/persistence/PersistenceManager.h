/**
 * @file PersistenceManager.h
 * @brief SQLite-based persistence for download state
 */

#pragma once

#include "openidm/engine/Types.h"
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <QObject>
#include <QSqlDatabase>

namespace OpenIDM {

class DownloadTask;
class Segment;

/**
 * @brief Task data for persistence
 */
struct TaskData {
    TaskId id;
    QString url;
    QString filePath;
    QString fileName;
    ByteCount totalSize;
    ByteCount downloadedSize;
    DownloadState state;
    bool supportsRanges;
    qint64 createdAt;
    qint64 updatedAt;
    QString contentType;
    QString errorMessage;
};

/**
 * @class PersistenceManager
 * @brief Manages persistent storage of download state using SQLite
 * 
 * Features:
 * - WAL mode for crash resilience
 * - Asynchronous writes to avoid blocking
 * - Periodic checkpoints
 * - Full task and segment state persistence
 */
class PersistenceManager : public QObject {
    Q_OBJECT

public:
    explicit PersistenceManager(QObject* parent = nullptr);
    ~PersistenceManager() override;
    
    /**
     * @brief Initialize the database
     * @param dbPath Path to database file (default: app data directory)
     * @return True if successful
     */
    bool initialize(const QString& dbPath = QString());
    
    /**
     * @brief Close database connection
     */
    void close();
    
    // ─────────────────────────────────────────────────────────────────────
    // Task Operations
    // ─────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Save a task to database
     * @param task Task to save
     */
    void saveTask(DownloadTask* task);
    
    /**
     * @brief Load all tasks from database
     * @return Vector of task data
     */
    std::vector<TaskData> loadAllTasks();
    
    /**
     * @brief Load a specific task
     * @param id Task ID
     * @return Task data or nullopt if not found
     */
    std::optional<TaskData> loadTask(const TaskId& id);
    
    /**
     * @brief Delete a task and its segments
     * @param id Task ID
     */
    void deleteTask(const TaskId& id);
    
    // ─────────────────────────────────────────────────────────────────────
    // Segment Operations
    // ─────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Save segment state
     * @param taskId Parent task ID
     * @param segment Segment to save
     */
    void saveSegment(const TaskId& taskId, Segment* segment);
    
    /**
     * @brief Save multiple segments
     * @param taskId Parent task ID
     * @param segments Segments to save
     */
    void saveSegments(const TaskId& taskId, const std::vector<Segment*>& segments);
    
    /**
     * @brief Load segments for a task
     * @param taskId Task ID
     * @return Vector of segment snapshots
     */
    std::vector<Segment::Snapshot> loadSegments(const TaskId& taskId);
    
    // ─────────────────────────────────────────────────────────────────────
    // Settings
    // ─────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Save a setting
     * @param key Setting key
     * @param value Setting value
     */
    void saveSetting(const QString& key, const QString& value);
    
    /**
     * @brief Load a setting
     * @param key Setting key
     * @param defaultValue Default if not found
     * @return Setting value
     */
    QString loadSetting(const QString& key, const QString& defaultValue = QString());
    
    // ─────────────────────────────────────────────────────────────────────
    // Maintenance
    // ─────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Force a WAL checkpoint
     */
    void checkpoint();
    
    /**
     * @brief Run VACUUM to reclaim space
     */
    void vacuum();
    
    /**
     * @brief Get database file path
     */
    QString databasePath() const { return m_dbPath; }
    
    /**
     * @brief Check if database is open
     */
    bool isOpen() const { return m_database.isOpen(); }
    
private:
    // Write operation types
    enum class WriteOp {
        SaveTask,
        SaveSegment,
        DeleteTask,
        SaveSetting
    };
    
    struct WriteRequest {
        WriteOp op;
        TaskId taskId;
        TaskData taskData;
        Segment::Snapshot segmentSnapshot;
        QString key;
        QString value;
    };
    
    // Database setup
    bool createSchema();
    bool migrateSchema();
    
    // Async write handling
    void writeThreadLoop();
    void processWrite(const WriteRequest& request);
    void enqueueWrite(WriteRequest request);
    
    // Internal operations
    void doSaveTask(const TaskData& data);
    void doSaveSegment(const TaskId& taskId, const Segment::Snapshot& snap);
    void doDeleteTask(const TaskId& id);
    void doSaveSetting(const QString& key, const QString& value);
    
    QSqlDatabase m_database;
    QString m_dbPath;
    
    // Async write queue
    std::queue<WriteRequest> m_writeQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::thread m_writeThread;
    std::atomic<bool> m_running{false};
};

} // namespace OpenIDM
