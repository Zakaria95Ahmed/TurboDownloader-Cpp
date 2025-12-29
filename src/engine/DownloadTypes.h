/**
 * @file DownloadTypes.h
 * @brief Core type definitions for the OpenIDM download engine
 *
 * This header defines all fundamental data structures and enumerations
 * used throughout the download engine. These types are designed to be:
 * - Thread-safe when wrapped appropriately
 * - Serializable for persistence
 * - QML-friendly via Q_ENUM and Q_GADGET
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_DOWNLOADTYPES_H
#define OPENIDM_DOWNLOADTYPES_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QUuid>
#include <QMetaType>

#include <optional>
#include <vector>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Constants
// ═══════════════════════════════════════════════════════════════════════════════

namespace Config {
    // Segmentation limits
    constexpr int MAX_SEGMENTS = 32;
    constexpr int DEFAULT_SEGMENTS = 8;
    constexpr qint64 MIN_SEGMENT_SIZE = 1 * 1024 * 1024;      // 1 MB minimum segment
    constexpr qint64 MIN_SPLIT_SIZE = 512 * 1024;              // 512 KB minimum for work-stealing

    // Buffer sizes
    constexpr size_t CURL_BUFFER_SIZE = 256 * 1024;            // 256 KB CURL buffer
    constexpr size_t FILE_WRITE_BUFFER = 1 * 1024 * 1024;      // 1 MB file write buffer

    // Timing
    constexpr int SCHEDULER_INTERVAL_MS = 100;                  // Work-stealing check interval
    constexpr int PROGRESS_UPDATE_INTERVAL_MS = 250;           // UI update interval
    constexpr int PERSISTENCE_INTERVAL_MS = 500;               // Database save interval
    constexpr int SPEED_SAMPLE_WINDOW_SECONDS = 5;             // Rolling average window

    // Retry configuration
    constexpr int MAX_RETRY_ATTEMPTS = 5;
    constexpr int INITIAL_RETRY_DELAY_MS = 1000;
    constexpr int MAX_RETRY_DELAY_MS = 30000;
    constexpr double RETRY_BACKOFF_MULTIPLIER = 2.0;

    // Network
    constexpr int CONNECTION_TIMEOUT_SECONDS = 30;
    constexpr int LOW_SPEED_LIMIT_BYTES = 1024;                // Abort if below 1KB/s
    constexpr int LOW_SPEED_TIME_SECONDS = 30;                 // for 30 seconds

    // Queue
    constexpr int DEFAULT_MAX_CONCURRENT_DOWNLOADS = 5;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Enumerations (QML-exposed)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief State machine states for a download task
 */
class DownloadStateClass {
    Q_GADGET
public:
    enum Value {
        Queued,           ///< Waiting in queue to start
        Resolving,        ///< Resolving streaming URL (yt-dlp)
        Connecting,       ///< Initial connection to server
        Downloading,      ///< Actively downloading segments
        Paused,           ///< User-paused
        Merging,          ///< Combining segment files
        Verifying,        ///< Checksum verification
        Completed,        ///< Successfully completed
        Error,            ///< Failed (see errorMessage)
        Cancelled         ///< User-cancelled
    };
    Q_ENUM(Value)
};
using DownloadState = DownloadStateClass::Value;

/**
 * @brief State of an individual download segment
 */
class SegmentStateClass {
    Q_GADGET
public:
    enum Value {
        Pending,          ///< Not yet started
        Active,           ///< Currently downloading
        Paused,           ///< Paused with parent task
        Completed,        ///< Segment fully downloaded
        Error             ///< Segment failed (will retry)
    };
    Q_ENUM(Value)
};
using SegmentState = SegmentStateClass::Value;

/**
 * @brief Download priority levels
 */
class DownloadPriorityClass {
    Q_GADGET
public:
    enum Value {
        Low = 0,
        Normal = 1,
        High = 2
    };
    Q_ENUM(Value)
};
using DownloadPriority = DownloadPriorityClass::Value;

/**
 * @brief Source type for URL resolution
 */
class UrlSourceTypeClass {
    Q_GADGET
public:
    enum Value {
        Direct,           ///< Direct HTTP(S) download
        YouTube,          ///< YouTube video/audio
        Streaming,        ///< Generic streaming (HLS/DASH)
        Torrent           ///< Torrent (future)
    };
    Q_ENUM(Value)
};
using UrlSourceType = UrlSourceTypeClass::Value;

// ═══════════════════════════════════════════════════════════════════════════════
// Data Structures
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Information about a single download segment
 *
 * Segments are the fundamental unit of parallel downloading.
 * Each segment downloads a byte range of the file independently.
 */
struct SegmentInfo {
    Q_GADGET
    Q_PROPERTY(int segmentId MEMBER segmentId)
    Q_PROPERTY(qint64 startByte MEMBER startByte)
    Q_PROPERTY(qint64 endByte MEMBER endByte)
    Q_PROPERTY(qint64 downloadedBytes MEMBER downloadedBytes)

public:
    int segmentId = 0;                  ///< Unique ID within parent task
    QString downloadId;                  ///< Parent download's UUID
    qint64 startByte = 0;               ///< First byte of range (inclusive)
    qint64 endByte = 0;                 ///< Last byte of range (inclusive)
    qint64 downloadedBytes = 0;         ///< Bytes downloaded so far
    SegmentState state = SegmentState::Pending;
    QString partFilePath;               ///< Path to .partN file
    QString partialChecksum;            ///< Rolling checksum for verification
    int retryCount = 0;                 ///< Number of retries attempted
    QDateTime lastActivity;             ///< Last successful data received

    /**
     * @brief Calculate remaining bytes to download
     */
    [[nodiscard]] qint64 remainingBytes() const noexcept {
        return (endByte - startByte + 1) - downloadedBytes;
    }

    /**
     * @brief Calculate completion percentage
     */
    [[nodiscard]] double progress() const noexcept {
        qint64 total = endByte - startByte + 1;
        return total > 0 ? (static_cast<double>(downloadedBytes) / total) * 100.0 : 0.0;
    }

    /**
     * @brief Check if segment can be split for work-stealing
     */
    [[nodiscard]] bool canSplit() const noexcept {
        return remainingBytes() >= Config::MIN_SPLIT_SIZE * 2;
    }

    /**
     * @brief Check if segment is complete
     */
    [[nodiscard]] bool isComplete() const noexcept {
        return downloadedBytes >= (endByte - startByte + 1);
    }
};

/**
 * @brief Complete information about a download task
 *
 * This structure holds all metadata for a single download,
 * persisted to SQLite and exposed to the UI layer.
 */
struct DownloadInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString fileName MEMBER fileName)
    Q_PROPERTY(qint64 totalSize MEMBER totalSize)
    Q_PROPERTY(qint64 downloadedBytes MEMBER downloadedBytes)

public:
    // Identity
    QString id;                          ///< UUID string
    QUrl originalUrl;                    ///< User-provided URL
    QUrl resolvedUrl;                    ///< Actual download URL (after yt-dlp)
    QString fileName;                    ///< Target file name
    QString savePath;                    ///< Full path to save directory
    UrlSourceType sourceType = UrlSourceType::Direct;

    // Size information
    qint64 totalSize = -1;              ///< Total file size (-1 if unknown)
    qint64 downloadedBytes = 0;         ///< Total bytes downloaded
    bool supportsRanges = false;        ///< Server supports byte ranges

    // State
    DownloadState state = DownloadState::Queued;
    DownloadPriority priority = DownloadPriority::Normal;
    QString errorMessage;                ///< Error description if state == Error

    // Segmentation
    int maxSegments = Config::DEFAULT_SEGMENTS;
    int activeSegments = 0;             ///< Currently active segment count
    std::vector<SegmentInfo> segments;  ///< Segment information

    // Timestamps
    QDateTime createdAt;
    QDateTime startedAt;
    QDateTime completedAt;
    QDateTime lastActivity;

    // Verification
    QString expectedChecksum;           ///< Expected SHA-256 (if known)
    QString actualChecksum;             ///< Computed SHA-256 after download

    // Statistics
    double averageSpeed = 0.0;          ///< Bytes per second (smoothed)
    qint64 peakSpeed = 0;               ///< Peak speed observed

    // Metadata (from HTTP headers)
    QString contentType;
    QString serverName;
    QDateTime lastModified;

    /**
     * @brief Calculate overall progress percentage
     */
    [[nodiscard]] double progress() const noexcept {
        if (totalSize <= 0) return 0.0;
        return (static_cast<double>(downloadedBytes) / totalSize) * 100.0;
    }

    /**
     * @brief Estimate time remaining in seconds
     */
    [[nodiscard]] qint64 estimatedTimeRemaining() const noexcept {
        if (averageSpeed <= 0 || totalSize <= 0) return -1;
        qint64 remaining = totalSize - downloadedBytes;
        return static_cast<qint64>(remaining / averageSpeed);
    }

    /**
     * @brief Get full file path
     */
    [[nodiscard]] QString fullFilePath() const {
        return savePath + "/" + fileName;
    }

    /**
     * @brief Check if download is active (not terminal state)
     */
    [[nodiscard]] bool isActive() const noexcept {
        return state == DownloadState::Downloading ||
               state == DownloadState::Connecting ||
               state == DownloadState::Resolving;
    }

    /**
     * @brief Check if download can be resumed
     */
    [[nodiscard]] bool canResume() const noexcept {
        return state == DownloadState::Paused ||
               state == DownloadState::Error;
    }

    /**
     * @brief Check if download is in terminal state
     */
    [[nodiscard]] bool isTerminal() const noexcept {
        return state == DownloadState::Completed ||
               state == DownloadState::Cancelled;
    }

    /**
     * @brief Create a new download with generated UUID
     */
    static DownloadInfo create(const QUrl& url, const QString& savePath) {
        DownloadInfo info;
        info.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        info.originalUrl = url;
        info.savePath = savePath;
        info.fileName = url.fileName();
        info.createdAt = QDateTime::currentDateTimeUtc();
        info.state = DownloadState::Queued;
        return info;
    }
};

/**
 * @brief Real-time statistics for a download
 *
 * Updated frequently for UI display.
 */
struct DownloadStats {
    Q_GADGET
    Q_PROPERTY(double speed MEMBER speed)
    Q_PROPERTY(qint64 eta MEMBER eta)

public:
    QString downloadId;
    qint64 downloadedBytes = 0;
    double speed = 0.0;                  ///< Current speed (bytes/sec)
    qint64 eta = -1;                     ///< Estimated seconds remaining
    int activeConnections = 0;           ///< Active segment count
    QDateTime timestamp;

    /**
     * @brief Format speed for display (e.g., "5.2 MB/s")
     */
    [[nodiscard]] QString formattedSpeed() const {
        if (speed < 1024) return QString::number(speed, 'f', 0) + " B/s";
        if (speed < 1024 * 1024) return QString::number(speed / 1024, 'f', 1) + " KB/s";
        if (speed < 1024 * 1024 * 1024) return QString::number(speed / (1024 * 1024), 'f', 2) + " MB/s";
        return QString::number(speed / (1024 * 1024 * 1024), 'f', 2) + " GB/s";
    }

    /**
     * @brief Format ETA for display (e.g., "2h 15m")
     */
    [[nodiscard]] QString formattedEta() const {
        if (eta < 0) return "Unknown";
        if (eta < 60) return QString::number(eta) + "s";
        if (eta < 3600) return QString::number(eta / 60) + "m " + QString::number(eta % 60) + "s";
        qint64 hours = eta / 3600;
        qint64 minutes = (eta % 3600) / 60;
        return QString::number(hours) + "h " + QString::number(minutes) + "m";
    }
};

/**
 * @brief Application settings
 */
struct Settings {
    Q_GADGET

public:
    // Download settings
    QString defaultSavePath;
    int maxConcurrentDownloads = Config::DEFAULT_MAX_CONCURRENT_DOWNLOADS;
    int maxSegmentsPerDownload = Config::DEFAULT_SEGMENTS;
    qint64 speedLimit = 0;              ///< Global speed limit (0 = unlimited)

    // Network settings
    bool useProxy = false;
    QString proxyHost;
    int proxyPort = 0;
    QString proxyUser;
    QString proxyPassword;

    // UI settings
    bool darkMode = true;
    bool minimizeToTray = true;
    bool showNotifications = true;
    bool startMinimized = false;
    bool autoStartDownloads = true;

    // Integration settings
    QString ytdlpPath;                  ///< Custom yt-dlp path (empty = auto)
    bool monitorClipboard = true;
};

/**
 * @brief HTTP header information from server
 */
struct HttpHeaderInfo {
    qint64 contentLength = -1;
    bool acceptRanges = false;
    QString contentType;
    QString contentDisposition;
    QString etag;
    QDateTime lastModified;
    QString server;

    /**
     * @brief Parse filename from Content-Disposition header
     */
    [[nodiscard]] std::optional<QString> parseFileName() const {
        if (contentDisposition.isEmpty()) return std::nullopt;

        // Parse: attachment; filename="example.pdf"
        QRegularExpression rx(R"(filename[*]?=['"]?([^'";\n]+)['"]?)");
        auto match = rx.match(contentDisposition);
        if (match.hasMatch()) {
            return match.captured(1);
        }
        return std::nullopt;
    }
};

} // namespace OpenIDM

// Register metatypes for Qt's signal/slot system
Q_DECLARE_METATYPE(OpenIDM::DownloadInfo)
Q_DECLARE_METATYPE(OpenIDM::SegmentInfo)
Q_DECLARE_METATYPE(OpenIDM::DownloadStats)
Q_DECLARE_METATYPE(OpenIDM::DownloadState)
Q_DECLARE_METATYPE(OpenIDM::SegmentState)

#endif // OPENIDM_DOWNLOADTYPES_H
