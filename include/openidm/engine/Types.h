/**
 * @file Types.h
 * @brief Core type definitions and enumerations for OpenIDM engine
 * 
 * This header defines fundamental types, enumerations, and constants used
 * throughout the download engine.
 */

#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <atomic>
#include <QString>
#include <QUuid>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Type Aliases
// ═══════════════════════════════════════════════════════════════════════════════

using TaskId = QUuid;
using SegmentId = uint32_t;
using ByteOffset = int64_t;
using ByteCount = int64_t;
using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;
using SpeedBps = double;  // Bytes per second

// ═══════════════════════════════════════════════════════════════════════════════
// Download State Machine
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Download task lifecycle states
 * 
 * State transitions:
 *   Queued → Probing → Downloading → Merging → Verifying → Completed
 *                ↓           ↓
 *              Paused ←→ Downloading
 *                ↓           ↓
 *              Failed      Failed
 */
enum class DownloadState : uint8_t {
    Queued,      ///< Waiting in queue to start
    Probing,     ///< Checking server capabilities (HEAD request)
    Downloading, ///< Actively downloading segments
    Paused,      ///< Paused by user, resumable
    Merging,     ///< Combining segment temp files
    Verifying,   ///< Checking file integrity (optional)
    Completed,   ///< Successfully finished
    Failed       ///< Unrecoverable error occurred
};

/**
 * @brief Segment download states
 */
enum class SegmentState : uint8_t {
    Pending,     ///< Not yet assigned to worker
    Active,      ///< Currently being downloaded
    Paused,      ///< Paused, can be resumed
    Completed,   ///< Segment finished successfully
    Failed,      ///< Segment error, may retry
    Stolen       ///< Split and reassigned by scheduler
};

/**
 * @brief Download priority levels
 */
enum class Priority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Urgent = 3
};

/**
 * @brief Error categories for download failures
 */
enum class ErrorCategory : uint8_t {
    None,
    Network,           ///< Connection issues
    ServerError,       ///< HTTP 5xx errors
    ClientError,       ///< HTTP 4xx errors
    FileSystem,        ///< Disk write errors
    Checksum,          ///< Integrity verification failed
    Cancelled,         ///< User cancelled
    Timeout,           ///< Operation timed out
    SSLError,          ///< Certificate validation failed
    Unknown
};

// ═══════════════════════════════════════════════════════════════════════════════
// Constants
// ═══════════════════════════════════════════════════════════════════════════════

namespace Constants {
    // Segment configuration
    constexpr size_t MAX_SEGMENTS = 32;
    constexpr size_t MIN_SEGMENTS = 1;
    constexpr size_t DEFAULT_SEGMENTS = 8;
    constexpr ByteCount MIN_SEGMENT_SIZE = 1 * 1024 * 1024;      // 1 MB
    constexpr ByteCount MIN_STEAL_SIZE = 512 * 1024;              // 512 KB
    constexpr ByteCount CHUNK_SIZE = 64 * 1024;                   // 64 KB
    
    // Download limits
    constexpr size_t MAX_CONCURRENT_DOWNLOADS = 8;
    constexpr size_t DEFAULT_CONCURRENT_DOWNLOADS = 3;
    
    // Timing intervals
    constexpr Duration PROGRESS_UPDATE_INTERVAL{100};             // 100ms
    constexpr Duration REBALANCE_INTERVAL{5000};                  // 5 seconds
    constexpr Duration PERSISTENCE_INTERVAL{5000};                // 5 seconds
    constexpr Duration SPEED_SAMPLE_INTERVAL{1000};               // 1 second
    constexpr Duration SPEED_SMOOTHING_WINDOW{10000};             // 10 seconds
    
    // Retry configuration
    constexpr size_t MAX_RETRIES = 5;
    constexpr Duration RETRY_BACKOFF_BASE{1000};                  // 1 second
    constexpr double RETRY_BACKOFF_MULTIPLIER = 2.0;
    constexpr Duration MAX_RETRY_DELAY{60000};                    // 1 minute
    
    // Network timeouts
    constexpr Duration CONNECT_TIMEOUT{30000};                    // 30 seconds
    constexpr Duration READ_TIMEOUT{60000};                       // 60 seconds
    constexpr Duration DNS_TIMEOUT{10000};                        // 10 seconds
    
    // File operations
    constexpr ByteCount PERSISTENCE_CHECKPOINT_BYTES = 1 * 1024 * 1024;  // 1 MB
    constexpr size_t FILE_BUFFER_SIZE = 256 * 1024;               // 256 KB
    
    // UI
    constexpr size_t SPEED_HISTORY_SIZE = 60;                     // 60 samples
    constexpr double ETA_SMOOTHING_FACTOR = 0.3;                  // Exponential smoothing
}

// ═══════════════════════════════════════════════════════════════════════════════
// Server Capabilities
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Information gathered from server probe (HEAD request)
 */
struct ServerCapabilities {
    bool supportsRanges = false;        ///< Accept-Ranges: bytes
    bool supportsCompression = false;   ///< Content-Encoding support
    ByteCount contentLength = -1;       ///< Total file size (-1 if unknown)
    QString contentType;                ///< MIME type
    QString fileName;                   ///< From Content-Disposition
    QString etag;                       ///< For resume validation
    QString lastModified;               ///< For resume validation
    int httpStatusCode = 0;             ///< Response status
    
    bool isValid() const { return httpStatusCode >= 200 && httpStatusCode < 400; }
    bool canSegment() const { return supportsRanges && contentLength > 0; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Progress Information
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Real-time progress information for a download
 */
struct DownloadProgress {
    ByteCount downloadedBytes = 0;      ///< Total bytes received
    ByteCount totalBytes = 0;           ///< Total file size
    SpeedBps currentSpeed = 0.0;        ///< Current download speed
    SpeedBps averageSpeed = 0.0;        ///< Average speed since start
    Duration remainingTime{0};          ///< Estimated time to completion
    double progressPercent = 0.0;       ///< 0.0 to 100.0
    size_t activeSegments = 0;          ///< Number of active workers
    size_t completedSegments = 0;       ///< Segments finished
    size_t totalSegments = 0;           ///< Total segment count
    
    bool isIndeterminate() const { return totalBytes <= 0; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Error Information
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Detailed error information for failures
 */
struct DownloadError {
    ErrorCategory category = ErrorCategory::None;
    int errorCode = 0;                  ///< Platform/library specific code
    QString message;                    ///< Human-readable description
    QString details;                    ///< Technical details for debugging
    Timestamp timestamp;                ///< When error occurred
    size_t retryCount = 0;              ///< Number of retries attempted
    
    bool isRecoverable() const {
        return category == ErrorCategory::Network ||
               category == ErrorCategory::Timeout ||
               category == ErrorCategory::ServerError;
    }
    
    bool hasError() const { return category != ErrorCategory::None; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Utility Functions
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Convert DownloadState to string for logging/display
 */
inline QString downloadStateToString(DownloadState state) {
    switch (state) {
        case DownloadState::Queued:      return QStringLiteral("Queued");
        case DownloadState::Probing:     return QStringLiteral("Probing");
        case DownloadState::Downloading: return QStringLiteral("Downloading");
        case DownloadState::Paused:      return QStringLiteral("Paused");
        case DownloadState::Merging:     return QStringLiteral("Merging");
        case DownloadState::Verifying:   return QStringLiteral("Verifying");
        case DownloadState::Completed:   return QStringLiteral("Completed");
        case DownloadState::Failed:      return QStringLiteral("Failed");
        default:                         return QStringLiteral("Unknown");
    }
}

/**
 * @brief Convert SegmentState to string
 */
inline QString segmentStateToString(SegmentState state) {
    switch (state) {
        case SegmentState::Pending:   return QStringLiteral("Pending");
        case SegmentState::Active:    return QStringLiteral("Active");
        case SegmentState::Paused:    return QStringLiteral("Paused");
        case SegmentState::Completed: return QStringLiteral("Completed");
        case SegmentState::Failed:    return QStringLiteral("Failed");
        case SegmentState::Stolen:    return QStringLiteral("Stolen");
        default:                      return QStringLiteral("Unknown");
    }
}

/**
 * @brief Format byte count for display (e.g., "1.5 GB")
 */
inline QString formatByteSize(ByteCount bytes) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;
    constexpr double TB = GB * 1024.0;
    
    if (bytes < 0) return QStringLiteral("Unknown");
    if (bytes < KB) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < MB) return QStringLiteral("%1 KB").arg(bytes / KB, 0, 'f', 1);
    if (bytes < GB) return QStringLiteral("%1 MB").arg(bytes / MB, 0, 'f', 2);
    if (bytes < TB) return QStringLiteral("%1 GB").arg(bytes / GB, 0, 'f', 2);
    return QStringLiteral("%1 TB").arg(bytes / TB, 0, 'f', 2);
}

/**
 * @brief Format speed for display (e.g., "1.5 MB/s")
 */
inline QString formatSpeed(SpeedBps speed) {
    return formatByteSize(static_cast<ByteCount>(speed)) + QStringLiteral("/s");
}

/**
 * @brief Format duration for display (e.g., "2h 15m 30s")
 */
inline QString formatDuration(Duration duration) {
    auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    
    if (totalSeconds < 0) return QStringLiteral("Unknown");
    if (totalSeconds == 0) return QStringLiteral("0s");
    
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
    QString result;
    if (hours > 0) result += QStringLiteral("%1h ").arg(hours);
    if (minutes > 0 || hours > 0) result += QStringLiteral("%1m ").arg(minutes);
    result += QStringLiteral("%1s").arg(seconds);
    
    return result.trimmed();
}

} // namespace OpenIDM
