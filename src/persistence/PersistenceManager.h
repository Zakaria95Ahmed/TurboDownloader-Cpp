/**
 * @file PersistenceManager.h
 * @brief SQLite-based persistence for downloads and settings
 *
 * Handles all database operations for storing and retrieving:
 * - Download metadata and state
 * - Segment information
 * - Application settings
 *
 * Uses WAL mode for better concurrent access and crash recovery.
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_PERSISTENCEMANAGER_H
#define OPENIDM_PERSISTENCEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QMutex>

#include <memory>
#include <vector>
#include <optional>

#include "engine/DownloadTypes.h"

namespace OpenIDM {

/**
 * @brief SQLite database manager for persistence
 *
 * Thread-safe wrapper around SQLite operations.
 * Uses a dedicated connection and batched writes for performance.
 */
class PersistenceManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param dbPath Path to database file (empty = default location)
     * @param parent QObject parent
     */
    explicit PersistenceManager(const QString& dbPath = QString(),
                                 QObject* parent = nullptr);

    ~PersistenceManager() override;

    // Non-copyable
    PersistenceManager(const PersistenceManager&) = delete;
    PersistenceManager& operator=(const PersistenceManager&) = delete;

    // ═══════════════════════════════════════════════════════════════════════════
    // Initialization
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Initialize the database
     * @return true on success
     */
    bool initialize();

    /**
     * @brief Check if database is ready
     */
    [[nodiscard]] bool isReady() const { return m_ready; }

    /**
     * @brief Get database path
     */
    [[nodiscard]] QString databasePath() const { return m_dbPath; }

    // ═══════════════════════════════════════════════════════════════════════════
    // Download Operations
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Save a download (insert or update)
     */
    bool saveDownload(const DownloadInfo& info);

    /**
     * @brief Load a download by ID
     */
    [[nodiscard]] std::optional<DownloadInfo> loadDownload(const QString& id);

    /**
     * @brief Load all downloads
     */
    [[nodiscard]] std::vector<DownloadInfo> loadAllDownloads();

    /**
     * @brief Delete a download
     */
    bool deleteDownload(const QString& id);

    /**
     * @brief Delete all downloads
     */
    bool deleteAllDownloads();

    // ═══════════════════════════════════════════════════════════════════════════
    // Segment Operations
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Save segments for a download
     */
    bool saveSegments(const QString& downloadId, const std::vector<SegmentInfo>& segments);

    /**
     * @brief Load segments for a download
     */
    [[nodiscard]] std::vector<SegmentInfo> loadSegments(const QString& downloadId);

    /**
     * @brief Delete segments for a download
     */
    bool deleteSegments(const QString& downloadId);

    // ═══════════════════════════════════════════════════════════════════════════
    // Settings Operations
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Save application settings
     */
    bool saveSettings(const Settings& settings);

    /**
     * @brief Load application settings
     */
    [[nodiscard]] Settings loadSettings();

    /**
     * @brief Save a single setting value
     */
    bool saveSetting(const QString& key, const QString& value);

    /**
     * @brief Load a single setting value
     */
    [[nodiscard]] QString loadSetting(const QString& key, const QString& defaultValue = QString());

    // ═══════════════════════════════════════════════════════════════════════════
    // Maintenance
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Vacuum the database to reclaim space
     */
    bool vacuum();

    /**
     * @brief Get database file size in bytes
     */
    [[nodiscard]] qint64 databaseSize() const;

signals:
    /**
     * @brief Emitted on database error
     */
    void error(const QString& message);

private:
    bool createTables();
    bool migrateDatabase(int fromVersion);
    int getDatabaseVersion();
    void setDatabaseVersion(int version);

    QString m_dbPath;
    QString m_connectionName;
    bool m_ready = false;

    mutable QMutex m_mutex;

    static constexpr int CURRENT_DB_VERSION = 1;
};

} // namespace OpenIDM

#endif // OPENIDM_PERSISTENCEMANAGER_H
