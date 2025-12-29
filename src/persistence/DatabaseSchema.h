/**
 * @file DatabaseSchema.h
 * @brief SQLite schema definitions
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_DATABASESCHEMA_H
#define OPENIDM_DATABASESCHEMA_H

namespace OpenIDM {
namespace DatabaseSchema {

// ═══════════════════════════════════════════════════════════════════════════════
// Downloads Table
// ═══════════════════════════════════════════════════════════════════════════════

constexpr const char* CREATE_DOWNLOADS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS downloads (
        id TEXT PRIMARY KEY NOT NULL,
        original_url TEXT NOT NULL,
        resolved_url TEXT,
        file_name TEXT NOT NULL,
        save_path TEXT NOT NULL,
        source_type INTEGER DEFAULT 0,
        total_size INTEGER DEFAULT -1,
        downloaded_bytes INTEGER DEFAULT 0,
        supports_ranges INTEGER DEFAULT 0,
        state INTEGER DEFAULT 0,
        priority INTEGER DEFAULT 1,
        error_message TEXT,
        max_segments INTEGER DEFAULT 8,
        active_segments INTEGER DEFAULT 0,
        created_at INTEGER NOT NULL,
        started_at INTEGER,
        completed_at INTEGER,
        last_activity INTEGER,
        expected_checksum TEXT,
        actual_checksum TEXT,
        average_speed REAL DEFAULT 0,
        peak_speed INTEGER DEFAULT 0,
        content_type TEXT,
        server_name TEXT,
        last_modified INTEGER
    )
)";

constexpr const char* CREATE_DOWNLOADS_STATE_INDEX = R"(
    CREATE INDEX IF NOT EXISTS idx_downloads_state ON downloads (state)
)";

// ═══════════════════════════════════════════════════════════════════════════════
// Segments Table
// ═══════════════════════════════════════════════════════════════════════════════

constexpr const char* CREATE_SEGMENTS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS segments (
        segment_id INTEGER NOT NULL,
        download_id TEXT NOT NULL,
        start_byte INTEGER NOT NULL,
        end_byte INTEGER NOT NULL,
        downloaded_bytes INTEGER DEFAULT 0,
        state INTEGER DEFAULT 0,
        part_file_path TEXT,
        partial_checksum TEXT,
        retry_count INTEGER DEFAULT 0,
        last_activity INTEGER,
        PRIMARY KEY (segment_id, download_id),
        FOREIGN KEY (download_id) REFERENCES downloads (id) ON DELETE CASCADE
    )
)";

constexpr const char* CREATE_SEGMENTS_DOWNLOAD_INDEX = R"(
    CREATE INDEX IF NOT EXISTS idx_segments_download ON segments (download_id)
)";

// ═══════════════════════════════════════════════════════════════════════════════
// Settings Table
// ═══════════════════════════════════════════════════════════════════════════════

constexpr const char* CREATE_SETTINGS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS settings (
        key TEXT PRIMARY KEY NOT NULL,
        value TEXT
    )
)";

} // namespace DatabaseSchema
} // namespace OpenIDM

#endif // OPENIDM_DATABASESCHEMA_H
