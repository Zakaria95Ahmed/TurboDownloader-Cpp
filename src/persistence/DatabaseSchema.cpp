/**
 * @file DatabaseSchema.cpp
 * @brief Database schema definitions and migrations
 */

#include <QStringList>

namespace OpenIDM {
namespace DatabaseSchema {

/**
 * @brief SQL statements for initial schema creation
 */
const QStringList SCHEMA_V1 = {
    // Downloads table
    QStringLiteral(R"(
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
            error_message   TEXT,
            priority        INTEGER DEFAULT 1,
            category        TEXT
        )
    )"),
    
    // Segments table
    QStringLiteral(R"(
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
    )"),
    
    // Settings table
    QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )"),
    
    // History table (for completed downloads)
    QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS history (
            id              TEXT PRIMARY KEY,
            url             TEXT NOT NULL,
            file_path       TEXT NOT NULL,
            file_name       TEXT NOT NULL,
            total_size      INTEGER,
            completed_at    INTEGER NOT NULL,
            duration_ms     INTEGER,
            average_speed   REAL
        )
    )"),
    
    // Indexes
    QStringLiteral("CREATE INDEX IF NOT EXISTS idx_downloads_state ON downloads(state)"),
    QStringLiteral("CREATE INDEX IF NOT EXISTS idx_downloads_created ON downloads(created_at DESC)"),
    QStringLiteral("CREATE INDEX IF NOT EXISTS idx_segments_download ON segments(download_id)"),
    QStringLiteral("CREATE INDEX IF NOT EXISTS idx_segments_state ON segments(download_id, state)"),
    QStringLiteral("CREATE INDEX IF NOT EXISTS idx_history_completed ON history(completed_at DESC)")
};

/**
 * @brief Pragmas for database configuration
 */
const QStringList PRAGMAS = {
    QStringLiteral("PRAGMA journal_mode = WAL"),
    QStringLiteral("PRAGMA synchronous = NORMAL"),
    QStringLiteral("PRAGMA foreign_keys = ON"),
    QStringLiteral("PRAGMA cache_size = -64000"),      // 64MB cache
    QStringLiteral("PRAGMA temp_store = MEMORY"),
    QStringLiteral("PRAGMA mmap_size = 268435456"),    // 256MB memory-mapped I/O
    QStringLiteral("PRAGMA page_size = 4096")
};

/**
 * @brief Schema version
 */
constexpr int CURRENT_SCHEMA_VERSION = 1;

/**
 * @brief Get schema version query
 */
const QString GET_SCHEMA_VERSION = QStringLiteral(
    "SELECT value FROM settings WHERE key = 'schema_version'"
);

/**
 * @brief Set schema version query
 */
const QString SET_SCHEMA_VERSION = QStringLiteral(
    "INSERT OR REPLACE INTO settings (key, value) VALUES ('schema_version', '%1')"
);

} // namespace DatabaseSchema
} // namespace OpenIDM
