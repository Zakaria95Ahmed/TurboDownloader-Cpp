/**
 * @file PersistenceManager.cpp
 * @brief SQLite persistence implementation
 */

#include "openidm/persistence/PersistenceManager.h"
#include "openidm/engine/DownloadTask.h"
#include "openidm/engine/Segment.h"

#include <QDebug>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QCoreApplication>

namespace OpenIDM {

PersistenceManager::PersistenceManager(QObject* parent)
    : QObject(parent)
{
}

PersistenceManager::~PersistenceManager() {
    close();
}

bool PersistenceManager::initialize(const QString& dbPath) {
    // Determine database path
    if (dbPath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dataDir.isEmpty()) {
            dataDir = QDir::homePath() + QStringLiteral("/.openidm");
        }
        QDir().mkpath(dataDir);
        m_dbPath = dataDir + QStringLiteral("/openidm.db");
    } else {
        m_dbPath = dbPath;
    }
    
    qDebug() << "PersistenceManager: Opening database at" << m_dbPath;
    
    // Open database
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("openidm"));
    m_database.setDatabaseName(m_dbPath);
    
    if (!m_database.open()) {
        qCritical() << "PersistenceManager: Failed to open database:"
                    << m_database.lastError().text();
        return false;
    }
    
    // Enable WAL mode and other pragmas
    QSqlQuery query(m_database);
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    query.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    query.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    query.exec(QStringLiteral("PRAGMA cache_size = -64000"));  // 64MB cache
    
    // Create schema
    if (!createSchema()) {
        qCritical() << "PersistenceManager: Failed to create schema";
        m_database.close();
        return false;
    }
    
    // Start write thread
    m_running = true;
    m_writeThread = std::thread(&PersistenceManager::writeThreadLoop, this);
    
    qDebug() << "PersistenceManager: Initialized successfully";
    return true;
}

void PersistenceManager::close() {
    // Stop write thread
    if (m_running) {
        m_running = false;
        m_queueCondition.notify_all();
        if (m_writeThread.joinable()) {
            m_writeThread.join();
        }
    }
    
    // Close database
    if (m_database.isOpen()) {
        m_database.close();
    }
    
    QSqlDatabase::removeDatabase(QStringLiteral("openidm"));
}

bool PersistenceManager::createSchema() {
    QSqlQuery query(m_database);
    
    // Downloads table
    bool success = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS downloads (
            id              TEXT PRIMARY KEY,
            url             TEXT NOT NULL,
            file_path       TEXT NOT NULL,
            file_name       TEXT NOT NULL,
            total_size      INTEGER NOT NULL DEFAULT -1,
            downloaded_size INTEGER DEFAULT 0,
            state           INTEGER NOT NULL DEFAULT 0,
            supports_ranges INTEGER DEFAULT 1,
            created_at      INTEGER NOT NULL,
            updated_at      INTEGER NOT NULL,
            completed_at    INTEGER,
            content_type    TEXT,
            checksum        TEXT,
            error_message   TEXT
        )
    )"));
    
    if (!success) {
        qCritical() << "PersistenceManager: Failed to create downloads table:"
                    << query.lastError().text();
        return false;
    }
    
    // Segments table
    success = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS segments (
            id              INTEGER,
            download_id     TEXT NOT NULL,
            segment_index   INTEGER NOT NULL,
            start_byte      INTEGER NOT NULL,
            end_byte        INTEGER NOT NULL,
            current_byte    INTEGER NOT NULL,
            state           INTEGER NOT NULL DEFAULT 0,
            checksum        INTEGER,
            temp_file       TEXT,
            retry_count     INTEGER DEFAULT 0,
            last_error      TEXT,
            PRIMARY KEY (download_id, id),
            FOREIGN KEY (download_id) REFERENCES downloads(id) ON DELETE CASCADE
        )
    )"));
    
    if (!success) {
        qCritical() << "PersistenceManager: Failed to create segments table:"
                    << query.lastError().text();
        return false;
    }
    
    // Settings table
    success = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )"));
    
    if (!success) {
        qCritical() << "PersistenceManager: Failed to create settings table:"
                    << query.lastError().text();
        return false;
    }
    
    // Indexes
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_downloads_state ON downloads(state)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_segments_download ON segments(download_id)"));
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Task Operations
// ═══════════════════════════════════════════════════════════════════════════════

void PersistenceManager::saveTask(DownloadTask* task) {
    if (!task) return;
    
    TaskData data;
    data.id = task->id();
    data.url = task->url();
    data.filePath = task->filePath();
    data.fileName = task->fileName();
    data.totalSize = task->totalSize();
    data.downloadedSize = task->downloadedSize();
    data.state = task->state();
    data.supportsRanges = task->supportsRanges();
    data.createdAt = QDateTime::currentMSecsSinceEpoch();
    data.updatedAt = QDateTime::currentMSecsSinceEpoch();
    data.contentType = task->contentType();
    data.errorMessage = task->errorMessage();
    
    WriteRequest request;
    request.op = WriteOp::SaveTask;
    request.taskId = task->id();
    request.taskData = data;
    
    enqueueWrite(std::move(request));
    
    // Also save segments
    auto segments = task->scheduler()->allSegments();
    saveSegments(task->id(), segments);
}

std::vector<TaskData> PersistenceManager::loadAllTasks() {
    std::vector<TaskData> result;
    
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        SELECT id, url, file_path, file_name, total_size, downloaded_size,
               state, supports_ranges, created_at, updated_at, content_type, error_message
        FROM downloads
        ORDER BY created_at DESC
    )"));
    
    if (!query.exec()) {
        qWarning() << "PersistenceManager: Failed to load tasks:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        TaskData data;
        data.id = QUuid::fromString(query.value(0).toString());
        data.url = query.value(1).toString();
        data.filePath = query.value(2).toString();
        data.fileName = query.value(3).toString();
        data.totalSize = query.value(4).toLongLong();
        data.downloadedSize = query.value(5).toLongLong();
        data.state = static_cast<DownloadState>(query.value(6).toInt());
        data.supportsRanges = query.value(7).toBool();
        data.createdAt = query.value(8).toLongLong();
        data.updatedAt = query.value(9).toLongLong();
        data.contentType = query.value(10).toString();
        data.errorMessage = query.value(11).toString();
        
        result.push_back(std::move(data));
    }
    
    return result;
}

std::optional<TaskData> PersistenceManager::loadTask(const TaskId& id) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        SELECT id, url, file_path, file_name, total_size, downloaded_size,
               state, supports_ranges, created_at, updated_at, content_type, error_message
        FROM downloads
        WHERE id = ?
    )"));
    query.addBindValue(id.toString(QUuid::WithoutBraces));
    
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    
    TaskData data;
    data.id = QUuid::fromString(query.value(0).toString());
    data.url = query.value(1).toString();
    data.filePath = query.value(2).toString();
    data.fileName = query.value(3).toString();
    data.totalSize = query.value(4).toLongLong();
    data.downloadedSize = query.value(5).toLongLong();
    data.state = static_cast<DownloadState>(query.value(6).toInt());
    data.supportsRanges = query.value(7).toBool();
    data.createdAt = query.value(8).toLongLong();
    data.updatedAt = query.value(9).toLongLong();
    data.contentType = query.value(10).toString();
    data.errorMessage = query.value(11).toString();
    
    return data;
}

void PersistenceManager::deleteTask(const TaskId& id) {
    WriteRequest request;
    request.op = WriteOp::DeleteTask;
    request.taskId = id;
    
    enqueueWrite(std::move(request));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Segment Operations
// ═══════════════════════════════════════════════════════════════════════════════

void PersistenceManager::saveSegment(const TaskId& taskId, Segment* segment) {
    if (!segment) return;
    
    WriteRequest request;
    request.op = WriteOp::SaveSegment;
    request.taskId = taskId;
    request.segmentSnapshot = segment->snapshot();
    
    enqueueWrite(std::move(request));
}

void PersistenceManager::saveSegments(const TaskId& taskId, const std::vector<Segment*>& segments) {
    for (Segment* seg : segments) {
        saveSegment(taskId, seg);
    }
}

std::vector<Segment::Snapshot> PersistenceManager::loadSegments(const TaskId& taskId) {
    std::vector<Segment::Snapshot> result;
    
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        SELECT id, start_byte, end_byte, current_byte, state, checksum, temp_file, retry_count, last_error
        FROM segments
        WHERE download_id = ?
        ORDER BY id
    )"));
    query.addBindValue(taskId.toString(QUuid::WithoutBraces));
    
    if (!query.exec()) {
        qWarning() << "PersistenceManager: Failed to load segments:" << query.lastError().text();
        return result;
    }
    
    while (query.next()) {
        Segment::Snapshot snap;
        snap.id = query.value(0).toUInt();
        snap.startByte = query.value(1).toLongLong();
        snap.endByte = query.value(2).toLongLong();
        snap.currentByte = query.value(3).toLongLong();
        snap.state = static_cast<SegmentState>(query.value(4).toInt());
        snap.checksum = query.value(5).toUInt();
        snap.tempFilePath = query.value(6).toString();
        snap.retryCount = query.value(7).toInt();
        snap.lastError = query.value(8).toString();
        
        result.push_back(std::move(snap));
    }
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Settings
// ═══════════════════════════════════════════════════════════════════════════════

void PersistenceManager::saveSetting(const QString& key, const QString& value) {
    WriteRequest request;
    request.op = WriteOp::SaveSetting;
    request.key = key;
    request.value = value;
    
    enqueueWrite(std::move(request));
}

QString PersistenceManager::loadSetting(const QString& key, const QString& defaultValue) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    
    return defaultValue;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Maintenance
// ═══════════════════════════════════════════════════════════════════════════════

void PersistenceManager::checkpoint() {
    QSqlQuery query(m_database);
    query.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
}

void PersistenceManager::vacuum() {
    checkpoint();
    QSqlQuery query(m_database);
    query.exec(QStringLiteral("VACUUM"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Async Write Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void PersistenceManager::enqueueWrite(WriteRequest request) {
    {
        std::lock_guard lock(m_queueMutex);
        m_writeQueue.push(std::move(request));
    }
    m_queueCondition.notify_one();
}

void PersistenceManager::writeThreadLoop() {
    qDebug() << "PersistenceManager: Write thread started";
    
    while (m_running || !m_writeQueue.empty()) {
        WriteRequest request;
        
        {
            std::unique_lock lock(m_queueMutex);
            m_queueCondition.wait(lock, [this]() {
                return !m_writeQueue.empty() || !m_running;
            });
            
            if (m_writeQueue.empty()) {
                continue;
            }
            
            request = std::move(m_writeQueue.front());
            m_writeQueue.pop();
        }
        
        processWrite(request);
    }
    
    qDebug() << "PersistenceManager: Write thread stopped";
}

void PersistenceManager::processWrite(const WriteRequest& request) {
    switch (request.op) {
        case WriteOp::SaveTask:
            doSaveTask(request.taskData);
            break;
        case WriteOp::SaveSegment:
            doSaveSegment(request.taskId, request.segmentSnapshot);
            break;
        case WriteOp::DeleteTask:
            doDeleteTask(request.taskId);
            break;
        case WriteOp::SaveSetting:
            doSaveSetting(request.key, request.value);
            break;
    }
}

void PersistenceManager::doSaveTask(const TaskData& data) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        INSERT OR REPLACE INTO downloads
        (id, url, file_path, file_name, total_size, downloaded_size, state,
         supports_ranges, created_at, updated_at, content_type, error_message)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));
    
    query.addBindValue(data.id.toString(QUuid::WithoutBraces));
    query.addBindValue(data.url);
    query.addBindValue(data.filePath);
    query.addBindValue(data.fileName);
    query.addBindValue(data.totalSize);
    query.addBindValue(data.downloadedSize);
    query.addBindValue(static_cast<int>(data.state));
    query.addBindValue(data.supportsRanges ? 1 : 0);
    query.addBindValue(data.createdAt);
    query.addBindValue(data.updatedAt);
    query.addBindValue(data.contentType);
    query.addBindValue(data.errorMessage);
    
    if (!query.exec()) {
        qWarning() << "PersistenceManager: Failed to save task:" << query.lastError().text();
    }
}

void PersistenceManager::doSaveSegment(const TaskId& taskId, const Segment::Snapshot& snap) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        INSERT OR REPLACE INTO segments
        (id, download_id, segment_index, start_byte, end_byte, current_byte,
         state, checksum, temp_file, retry_count, last_error)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));
    
    query.addBindValue(snap.id);
    query.addBindValue(taskId.toString(QUuid::WithoutBraces));
    query.addBindValue(snap.id);  // segment_index same as id for now
    query.addBindValue(snap.startByte);
    query.addBindValue(snap.endByte);
    query.addBindValue(snap.currentByte);
    query.addBindValue(static_cast<int>(snap.state));
    query.addBindValue(snap.checksum);
    query.addBindValue(snap.tempFilePath);
    query.addBindValue(snap.retryCount);
    query.addBindValue(snap.lastError);
    
    if (!query.exec()) {
        qWarning() << "PersistenceManager: Failed to save segment:" << query.lastError().text();
    }
}

void PersistenceManager::doDeleteTask(const TaskId& id) {
    QSqlQuery query(m_database);
    
    // Delete segments first (foreign key)
    query.prepare(QStringLiteral("DELETE FROM segments WHERE download_id = ?"));
    query.addBindValue(id.toString(QUuid::WithoutBraces));
    query.exec();
    
    // Delete task
    query.prepare(QStringLiteral("DELETE FROM downloads WHERE id = ?"));
    query.addBindValue(id.toString(QUuid::WithoutBraces));
    
    if (!query.exec()) {
        qWarning() << "PersistenceManager: Failed to delete task:" << query.lastError().text();
    }
}

void PersistenceManager::doSaveSetting(const QString& key, const QString& value) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    
    if (!query.exec()) {
        qWarning() << "PersistenceManager: Failed to save setting:" << query.lastError().text();
    }
}

} // namespace OpenIDM
