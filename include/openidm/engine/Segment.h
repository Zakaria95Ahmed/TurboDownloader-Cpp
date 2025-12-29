/**
 * @file Segment.h
 * @brief Segment data structure for download chunk management
 * 
 * A Segment represents a contiguous byte range of a download file.
 * Multiple segments are downloaded in parallel and merged upon completion.
 */

#pragma once

#include "openidm/engine/Types.h"
#include <atomic>
#include <memory>
#include <utility>
#include <QFile>

namespace OpenIDM {

/**
 * @class Segment
 * @brief Represents a downloadable byte range of a file
 * 
 * Segments are the fundamental unit of parallel downloading. Each segment:
 * - Has a fixed start and (adjustable) end byte position
 * - Maintains atomic progress tracking for thread-safe updates
 * - Can be split into smaller segments for work-stealing
 * - Stores a rolling checksum for corruption detection
 * 
 * Thread Safety:
 * - currentByte and state are atomic for lock-free progress updates
 * - Other members should only be modified by the owning thread/scheduler
 */
class Segment {
public:
    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Construct a new segment
     * @param id Unique segment identifier within the download task
     * @param startByte First byte of the range (inclusive)
     * @param endByte Last byte of the range (inclusive)
     */
    Segment(SegmentId id, ByteOffset startByte, ByteOffset endByte);
    
    /// Default constructor for container compatibility
    Segment();
    
    /// Move constructor
    Segment(Segment&& other) noexcept;
    
    /// Move assignment
    Segment& operator=(Segment&& other) noexcept;
    
    // Copy operations disabled due to atomic members
    Segment(const Segment&) = delete;
    Segment& operator=(const Segment&) = delete;
    
    ~Segment() = default;
    
    // ───────────────────────────────────────────────────────────────────────
    // Identification
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Unique segment ID within the parent task
    SegmentId id() const { return m_id; }
    
    // ───────────────────────────────────────────────────────────────────────
    // Byte Range
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return First byte position (inclusive)
    ByteOffset startByte() const { return m_startByte; }
    
    /// @return Last byte position (inclusive)
    ByteOffset endByte() const { return m_endByte; }
    
    /// @return Total size of this segment in bytes
    ByteCount totalSize() const { return m_endByte - m_startByte + 1; }
    
    /// @return Current download position (atomic read)
    ByteOffset currentByte() const { return m_currentByte.load(std::memory_order_relaxed); }
    
    /// @return Number of bytes already downloaded
    ByteCount downloadedBytes() const { return currentByte() - m_startByte; }
    
    /// @return Number of bytes remaining to download
    ByteCount remainingBytes() const { return m_endByte - currentByte() + 1; }
    
    /// @return Progress as fraction (0.0 to 1.0)
    double progress() const {
        auto total = totalSize();
        return total > 0 ? static_cast<double>(downloadedBytes()) / total : 0.0;
    }
    
    /**
     * @brief Update current byte position (thread-safe)
     * @param position New position
     */
    void setCurrentByte(ByteOffset position) {
        m_currentByte.store(position, std::memory_order_relaxed);
    }
    
    /**
     * @brief Atomically advance current position
     * @param bytes Number of bytes to advance
     * @return New position after advancement
     */
    ByteOffset advanceBy(ByteCount bytes) {
        return m_currentByte.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    }
    
    /**
     * @brief Adjust the end byte (for work-stealing splits)
     * @param newEnd New end byte position (must be >= currentByte)
     */
    void setEndByte(ByteOffset newEnd);
    
    // ───────────────────────────────────────────────────────────────────────
    // State Management
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Current segment state (atomic read)
    SegmentState state() const { return m_state.load(std::memory_order_acquire); }
    
    /// @return True if segment is actively downloading
    bool isActive() const { return state() == SegmentState::Active; }
    
    /// @return True if segment has completed successfully
    bool isComplete() const { return state() == SegmentState::Completed; }
    
    /// @return True if segment can accept more work
    bool isPending() const { return state() == SegmentState::Pending; }
    
    /// @return True if segment encountered an error
    bool isFailed() const { return state() == SegmentState::Failed; }
    
    /**
     * @brief Update segment state (thread-safe)
     * @param newState New state value
     */
    void setState(SegmentState newState) {
        m_state.store(newState, std::memory_order_release);
    }
    
    /**
     * @brief Attempt state transition (CAS operation)
     * @param expected Expected current state
     * @param desired Desired new state
     * @return True if transition succeeded
     */
    bool trySetState(SegmentState expected, SegmentState desired) {
        return m_state.compare_exchange_strong(expected, desired,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }
    
    // ───────────────────────────────────────────────────────────────────────
    // Integrity & Checksum
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Current CRC32 checksum of downloaded data
    uint32_t checksum() const { return m_checksum; }
    
    /**
     * @brief Update rolling checksum with new data
     * @param data Pointer to data buffer
     * @param size Size of data
     */
    void updateChecksum(const char* data, size_t size);
    
    /// @brief Reset checksum to initial value
    void resetChecksum() { m_checksum = 0; }
    
    // ───────────────────────────────────────────────────────────────────────
    // Temporary File
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Path to segment's temporary file
    QString tempFilePath() const { return m_tempFilePath; }
    
    /**
     * @brief Set temporary file path
     * @param path Full path to temp file
     */
    void setTempFilePath(const QString& path) { m_tempFilePath = path; }
    
    // ───────────────────────────────────────────────────────────────────────
    // Error Handling & Retry
    // ───────────────────────────────────────────────────────────────────────
    
    /// @return Number of retry attempts made
    int retryCount() const { return m_retryCount; }
    
    /// @brief Increment retry counter
    void incrementRetry() { ++m_retryCount; }
    
    /// @brief Reset retry counter
    void resetRetry() { m_retryCount = 0; }
    
    /// @return True if more retries are allowed
    bool canRetry() const { return m_retryCount < Constants::MAX_RETRIES; }
    
    /// @return Last error message
    QString lastError() const { return m_lastError; }
    
    /**
     * @brief Record an error
     * @param message Error description
     */
    void setLastError(const QString& message) { m_lastError = message; }
    
    // ───────────────────────────────────────────────────────────────────────
    // Work Stealing
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Check if segment is large enough to split
     * @param minSize Minimum remaining bytes required for splitting
     * @return True if segment can be split
     */
    bool isSplittable(ByteCount minSize = Constants::MIN_STEAL_SIZE) const {
        return remainingBytes() >= minSize * 2;
    }
    
    /**
     * @brief Split this segment into two parts
     * 
     * Splits the remaining work in half:
     * - This segment's end is updated to midpoint-1
     * - New segment covers midpoint to original end
     * 
     * @param newId ID for the new segment
     * @return New segment covering second half, or nullopt if not splittable
     */
    std::unique_ptr<Segment> split(SegmentId newId);
    
    // ───────────────────────────────────────────────────────────────────────
    // HTTP Range Header
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Generate HTTP Range header value
     * @return String like "bytes=1000-1999" for Range header
     */
    QString rangeHeader() const {
        return QStringLiteral("bytes=%1-%2").arg(currentByte()).arg(m_endByte);
    }
    
    // ───────────────────────────────────────────────────────────────────────
    // Serialization (for persistence)
    // ───────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Create a snapshot of current state for persistence
     * @return Copy-safe data for database storage
     */
    struct Snapshot {
        SegmentId id;
        ByteOffset startByte;
        ByteOffset endByte;
        ByteOffset currentByte;
        SegmentState state;
        uint32_t checksum;
        QString tempFilePath;
        int retryCount;
        QString lastError;
    };
    
    Snapshot snapshot() const;
    
    /**
     * @brief Restore segment from snapshot
     * @param snap Snapshot data from database
     */
    void restore(const Snapshot& snap);
    
private:
    // Identification
    SegmentId m_id{0};
    
    // Byte range (fixed at creation, endByte adjustable for splits)
    ByteOffset m_startByte{0};
    ByteOffset m_endByte{0};
    
    // Progress tracking (atomic for thread-safe updates)
    std::atomic<ByteOffset> m_currentByte{0};
    std::atomic<SegmentState> m_state{SegmentState::Pending};
    
    // Integrity
    uint32_t m_checksum{0};
    
    // Temporary storage
    QString m_tempFilePath;
    
    // Error handling
    int m_retryCount{0};
    QString m_lastError;
};

/**
 * @brief Calculate CRC32 checksum
 * @param data Input data
 * @param size Data size
 * @param previousCrc Previous checksum for rolling calculation
 * @return Updated CRC32 value
 */
uint32_t calculateCRC32(const char* data, size_t size, uint32_t previousCrc = 0);

} // namespace OpenIDM
