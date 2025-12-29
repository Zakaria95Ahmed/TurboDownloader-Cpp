# OpenIDM Architecture Design Document

## Executive Summary

OpenIDM is a high-performance, cross-platform download manager built with modern C++20 and Qt 6 Quick/QML. This document details the complete software architecture, focusing on the download engine design, threading model, and dynamic segmentation strategy.

---

## Table of Contents

1. [System Architecture Overview](#1-system-architecture-overview)
2. [Download Engine Class Hierarchy](#2-download-engine-class-hierarchy)
3. [Threading Model & Synchronization](#3-threading-model--synchronization)
4. [Dynamic Segmentation Algorithm](#4-dynamic-segmentation-algorithm)
5. [Persistence Layer Design](#5-persistence-layer-design)
6. [UI/MVVM Architecture](#6-uimvvm-architecture)
7. [Platform Abstraction](#7-platform-abstraction)

---

## 1. System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              OpenIDM Architecture                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        QML UI Layer (View)                           │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │   │
│  │  │  Main.qml   │  │DownloadList │  │  AddDialog  │  │ SettingsPage│ │   │
│  │  │             │  │   .qml      │  │    .qml     │  │    .qml     │ │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    ViewModel Layer (C++ / Qt)                        │   │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │   │
│  │  │DownloadListModel│  │DownloadViewModel│  │  SettingsViewModel  │  │   │
│  │  │  (QAbstractList │  │   (Q_OBJECT)    │  │    (Q_OBJECT)       │  │   │
│  │  │     Model)      │  │                 │  │                     │  │   │
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Download Engine Core                            │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐   │   │
│  │  │DownloadManager│─▶│DownloadTask  │─▶│   SegmentScheduler       │   │   │
│  │  │   (Singleton) │  │  (per URL)   │  │  (Work-Stealing Queue)   │   │   │
│  │  └──────────────┘  └──────────────┘  └──────────────────────────┘   │   │
│  │          │                │                      │                   │   │
│  │          │                ▼                      ▼                   │   │
│  │          │         ┌──────────────┐      ┌──────────────┐           │   │
│  │          │         │SegmentWorker │──────│SegmentWorker │ ...       │   │
│  │          │         │  (Thread 1)  │      │  (Thread N)  │           │   │
│  │          │         └──────────────┘      └──────────────┘           │   │
│  │          │                │                      │                   │   │
│  │          ▼                └──────────┬───────────┘                   │   │
│  │  ┌──────────────┐                    │                               │   │
│  │  │ Persistence  │◀───────────────────┘                               │   │
│  │  │   Manager    │                                                    │   │
│  │  │  (SQLite)    │                                                    │   │
│  │  └──────────────┘                                                    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Platform Abstraction Layer                      │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │   │
│  │  │   Windows   │  │    Linux    │  │    macOS    │  │   Android   │ │   │
│  │  │  (WinAPI)   │  │  (D-Bus)    │  │  (AppKit)   │  │   (JNI)     │ │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Download Engine Class Hierarchy

### 2.1 Complete Class Diagram

```
                                    ┌─────────────────────────┐
                                    │     DownloadManager     │
                                    │  (Singleton, Q_OBJECT)  │
                                    ├─────────────────────────┤
                                    │ - m_tasks: TaskMap      │
                                    │ - m_scheduler: unique_ptr│
                                    │ - m_persistence: ptr    │
                                    │ - m_threadPool: ptr     │
                                    │ - m_maxConcurrent: int  │
                                    ├─────────────────────────┤
                                    │ + addDownload(url)      │
                                    │ + pauseDownload(id)     │
                                    │ + resumeDownload(id)    │
                                    │ + cancelDownload(id)    │
                                    │ + getTask(id): Task*    │
                                    │ + getAllTasks(): vector │
                                    └───────────┬─────────────┘
                                                │ owns 1..*
                                                ▼
                           ┌─────────────────────────────────────────┐
                           │              DownloadTask               │
                           │            (QObject, per-file)          │
                           ├─────────────────────────────────────────┤
                           │ - m_id: UUID                            │
                           │ - m_url: QString                        │
                           │ - m_filePath: QString                   │
                           │ - m_totalSize: int64_t                  │
                           │ - m_downloadedBytes: atomic<int64_t>    │
                           │ - m_state: DownloadState                │
                           │ - m_segments: vector<Segment>           │
                           │ - m_scheduler: SegmentScheduler*        │
                           │ - m_supportsRanges: bool                │
                           ├─────────────────────────────────────────┤
                           │ + start()                               │
                           │ + pause()                               │
                           │ + resume()                              │
                           │ + cancel()                              │
                           │ + probeServerCapabilities()             │
                           │ + initializeSegments(count)             │
                           │ + mergeSegments()                       │
                           │ + calculateProgress(): double           │
                           │ + calculateSpeed(): double              │
                           │ + calculateETA(): seconds               │
                           │ signals: progressChanged(), stateChanged│
                           └───────────┬─────────────────────────────┘
                                       │ contains 1..*
                                       ▼
                     ┌───────────────────────────────────────────────┐
                     │                   Segment                      │
                     │              (POD / Value Type)                │
                     ├───────────────────────────────────────────────┤
                     │ + id: uint32_t                                │
                     │ + startByte: int64_t                          │
                     │ + endByte: int64_t                            │
                     │ + currentByte: atomic<int64_t>                │
                     │ + state: SegmentState                         │
                     │ + checksum: uint32_t (CRC32)                  │
                     │ + tempFilePath: QString                       │
                     │ + retryCount: int                             │
                     │ + lastError: QString                          │
                     ├───────────────────────────────────────────────┤
                     │ + remainingBytes(): int64_t                   │
                     │ + progress(): double                          │
                     │ + isComplete(): bool                          │
                     │ + isSplittable(minSize): bool                 │
                     │ + split(): pair<Segment, Segment>             │
                     └───────────────────────────────────────────────┘

┌─────────────────────────────────────┐     ┌─────────────────────────────────────┐
│         SegmentScheduler            │     │           SegmentWorker             │
│      (Work-Stealing Queue)          │     │         (QRunnable/Thread)          │
├─────────────────────────────────────┤     ├─────────────────────────────────────┤
│ - m_pendingQueue: deque<Segment*>   │     │ - m_segment: Segment*               │
│ - m_activeWorkers: set<Worker*>     │     │ - m_curl: CURL*                     │
│ - m_mutex: shared_mutex             │     │ - m_task: DownloadTask*             │
│ - m_condition: condition_variable   │     │ - m_scheduler: SegmentScheduler*    │
│ - m_throughputMap: map<Worker, bps> │     │ - m_running: atomic<bool>           │
│ - m_rebalanceTimer: Timer           │     │ - m_bytesThisSecond: atomic<int64_t>│
├─────────────────────────────────────┤     ├─────────────────────────────────────┤
│ + scheduleSegment(Segment*)         │     │ + run() override                    │
│ + stealWork(): Segment*             │     │ + pause()                           │
│ + onSegmentComplete(Worker*)        │     │ + resume()                          │
│ + rebalanceSegments()               │     │ + stop()                            │
│ + splitLargestSegment()             │     │ - downloadChunk(): size_t           │
│ + getOptimalSegmentCount(): int     │     │ - handleError(CURLcode)             │
│ + notifyWorkerIdle(Worker*)         │     │ - updateProgress()                  │
└─────────────────────────────────────┘     │ signals: chunkReceived(), error()   │
                                            └─────────────────────────────────────┘

┌─────────────────────────────────────┐     ┌─────────────────────────────────────┐
│        PersistenceManager           │     │          NetworkProbe               │
│           (SQLite + WAL)            │     │     (Server Capability Detection)   │
├─────────────────────────────────────┤     ├─────────────────────────────────────┤
│ - m_database: QSqlDatabase          │     │ - m_curl: CURL*                     │
│ - m_writeQueue: queue<WriteOp>      │     ├─────────────────────────────────────┤
│ - m_writeThread: thread             │     │ + probe(url): ServerCapabilities    │
│ - m_checkpointInterval: ms          │     │ + supportsRanges(): bool            │
├─────────────────────────────────────┤     │ + getContentLength(): int64_t       │
│ + saveTask(DownloadTask*)           │     │ + getFileName(): QString            │
│ + saveSegment(Segment*)             │     │ + getContentType(): QString         │
│ + loadAllTasks(): vector<Task>      │     │ + getLastModified(): QDateTime      │
│ + loadSegments(taskId): vector<Seg> │     │ + supportsCompression(): bool       │
│ + deleteTask(taskId)                │     └─────────────────────────────────────┘
│ + checkpoint()                      │
│ + vacuum()                          │
└─────────────────────────────────────┘
```

### 2.2 State Enumerations

```cpp
enum class DownloadState {
    Queued,      // Waiting to start
    Probing,     // Checking server capabilities
    Downloading, // Actively downloading
    Paused,      // User paused
    Completed,   // Successfully finished
    Failed,      // Unrecoverable error
    Merging,     // Combining segment files
    Verifying    // Checking integrity
};

enum class SegmentState {
    Pending,     // Not yet started
    Active,      // Currently downloading
    Paused,      // Paused by user or scheduler
    Completed,   // Finished successfully
    Failed,      // Error occurred
    Stolen       // Taken by work-stealing
};
```

---

## 3. Threading Model & Synchronization

### 3.1 Thread Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           OpenIDM Thread Model                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        Main Thread (Qt Event Loop)                   │   │
│  │  - UI rendering and event handling                                   │   │
│  │  - Signal/Slot connections                                           │   │
│  │  - DownloadManager coordination                                      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                         │                                   │
│                    ┌────────────────────┼────────────────────┐             │
│                    ▼                    ▼                    ▼             │
│  ┌────────────────────────┐  ┌──────────────────┐  ┌────────────────────┐ │
│  │   Download Thread Pool │  │ Persistence Thread│  │  UI Update Timer  │ │
│  │   (QThreadPool-based)  │  │  (Single Writer)  │  │  (100ms interval) │ │
│  │                        │  │                   │  │                   │ │
│  │  ┌─────────────────┐   │  │  - SQLite writes  │  │  - Batch UI       │ │
│  │  │ SegmentWorker 1 │   │  │  - WAL checkpoints│  │    updates        │ │
│  │  │ (curl handle)   │   │  │  - Atomic commits │  │  - Progress       │ │
│  │  └─────────────────┘   │  │                   │  │    aggregation    │ │
│  │  ┌─────────────────┐   │  └──────────────────┘  │  - ETA smoothing  │ │
│  │  │ SegmentWorker 2 │   │                        └────────────────────┘ │
│  │  │ (curl handle)   │   │                                               │
│  │  └─────────────────┘   │                                               │
│  │        ...             │                                               │
│  │  ┌─────────────────┐   │                                               │
│  │  │ SegmentWorker N │   │   N = min(32, hardware_concurrency * 2)      │
│  │  │ (curl handle)   │   │                                               │
│  │  └─────────────────┘   │                                               │
│  └────────────────────────┘                                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Synchronization Strategy

```cpp
// Lock-free progress updates using atomics
class Segment {
    std::atomic<int64_t> m_currentByte{0};      // Updated by worker threads
    std::atomic<SegmentState> m_state{Pending}; // State machine transitions
};

// Shared mutex for read-heavy workloads (segment queries)
class SegmentScheduler {
    mutable std::shared_mutex m_mutex;
    
    // Multiple readers for progress queries
    auto getActiveSegments() const {
        std::shared_lock lock(m_mutex);
        return m_activeSegments;
    }
    
    // Exclusive write for segment modifications
    void splitSegment(Segment* seg) {
        std::unique_lock lock(m_mutex);
        // ... split logic
    }
};

// Lock-free SPSC queue for persistence writes
class PersistenceManager {
    moodycamel::ReaderWriterQueue<WriteOperation> m_writeQueue;
    
    void enqueueWrite(WriteOperation op) {
        m_writeQueue.enqueue(std::move(op));
    }
};
```

### 3.3 Signal Flow for Progress Updates

```
SegmentWorker::downloadChunk()
    │
    ▼
┌───────────────────────────────────┐
│ segment.currentByte.fetch_add(n)  │  ◀── Atomic, lock-free
└───────────────────────────────────┘
    │
    ▼ (Every 100ms via Timer)
┌───────────────────────────────────┐
│ DownloadTask::aggregateProgress() │  ◀── Batch read of all segments
└───────────────────────────────────┘
    │
    ▼ (Qt Signal, queued connection)
┌───────────────────────────────────┐
│ DownloadListModel::updateProgress │  ◀── Main thread only
└───────────────────────────────────┘
    │
    ▼
┌───────────────────────────────────┐
│ QML Binding auto-updates UI       │
└───────────────────────────────────┘
```

---

## 4. Dynamic Segmentation Algorithm

### 4.1 Algorithm Overview

The dynamic segmentation algorithm maximizes bandwidth utilization by:

1. **Initial Segmentation**: Divide file into N segments based on file size and connection quality
2. **Parallel Download**: Start workers for each segment
3. **Work Stealing**: When a worker finishes, it steals work from the slowest segment
4. **Dynamic Re-segmentation**: Split large remaining segments to keep all workers busy
5. **Throughput Balancing**: Monitor per-segment speeds and rebalance accordingly

### 4.2 Detailed Algorithm

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Dynamic Segmentation Algorithm                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  PHASE 1: INITIALIZATION                                                    │
│  ─────────────────────────                                                  │
│                                                                             │
│  1. Probe server with HEAD request                                          │
│     ├─ Check Accept-Ranges header                                           │
│     ├─ Get Content-Length                                                   │
│     └─ Determine if segmentation is possible                                │
│                                                                             │
│  2. Calculate optimal segment count:                                        │
│     ┌─────────────────────────────────────────────────────────────────┐    │
│     │ segmentCount = min(                                              │    │
│     │     MAX_SEGMENTS,           // 32                                │    │
│     │     max(                                                         │    │
│     │         MIN_SEGMENTS,       // 1                                 │    │
│     │         fileSize / MIN_SEGMENT_SIZE  // 1MB minimum per segment  │    │
│     │     )                                                            │    │
│     │ )                                                                │    │
│     └─────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  3. Create initial segments:                                                │
│     ┌─────────────────────────────────────────────────────────────────┐    │
│     │ segmentSize = fileSize / segmentCount                            │    │
│     │ for i in 0..segmentCount:                                        │    │
│     │     start = i * segmentSize                                      │    │
│     │     end = (i == last) ? fileSize - 1 : start + segmentSize - 1   │    │
│     │     segments.push(Segment{start, end})                           │    │
│     └─────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  PHASE 2: PARALLEL DOWNLOAD                                                 │
│  ──────────────────────────                                                 │
│                                                                             │
│  For each segment, spawn a SegmentWorker:                                   │
│     ┌─────────────────────────────────────────────────────────────────┐    │
│     │ while (segment.currentByte < segment.endByte):                   │    │
│     │     chunk = curl_read(segment.currentByte, CHUNK_SIZE)           │    │
│     │     file.write(chunk)                                            │    │
│     │     segment.currentByte += chunk.size                            │    │
│     │     segment.checksum.update(chunk)                               │    │
│     │                                                                  │    │
│     │     if (scheduler.shouldYield()):                                │    │
│     │         scheduler.notifyProgress(segment)                        │    │
│     │         yield()  // Allow scheduler to reassign if needed        │    │
│     └─────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  PHASE 3: WORK STEALING                                                     │
│  ──────────────────────                                                     │
│                                                                             │
│  When a worker completes its segment:                                       │
│     ┌─────────────────────────────────────────────────────────────────┐    │
│     │ 1. Find the largest remaining segment (by bytes remaining)       │    │
│     │                                                                  │    │
│     │ 2. If largest.remainingBytes > MIN_STEAL_SIZE (512KB):          │    │
│     │    a. Pause the slow worker                                      │    │
│     │    b. Calculate split point:                                     │    │
│     │       splitPoint = largest.currentByte +                         │    │
│     │                    (largest.remainingBytes / 2)                  │    │
│     │    c. Create new segment: [splitPoint, largest.endByte]          │    │
│     │    d. Update original: largest.endByte = splitPoint - 1          │    │
│     │    e. Resume slow worker with reduced range                      │    │
│     │    f. Assign new segment to idle worker                          │    │
│     │                                                                  │    │
│     │ 3. Else: Worker becomes idle, waits for more work                │    │
│     └─────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  PHASE 4: THROUGHPUT REBALANCING                                           │
│  ─────────────────────────────                                              │
│                                                                             │
│  Every REBALANCE_INTERVAL (5 seconds):                                      │
│     ┌─────────────────────────────────────────────────────────────────┐    │
│     │ 1. Calculate throughput for each active segment                  │    │
│     │    throughput[i] = bytesDownloaded[i] / elapsed                  │    │
│     │                                                                  │    │
│     │ 2. Identify slow segments (< 50% of average throughput)          │    │
│     │                                                                  │    │
│     │ 3. For each slow segment with significant remaining bytes:       │    │
│     │    - If likely server throttling: create new connection          │    │
│     │    - If network issue: retry with exponential backoff            │    │
│     │    - Split and redistribute to faster workers                    │    │
│     │                                                                  │    │
│     │ 4. Update ETA estimation with smoothed average                   │    │
│     └─────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Work-Stealing Visualization

```
Initial State: 100MB file, 4 segments
═══════════════════════════════════════════════════════════════════════════

Segment 1: [████████████████████████░░░░░░░░░░░░] 0-25MB   (Worker A: Fast)
Segment 2: [████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░] 25-50MB  (Worker B: Slow)
Segment 3: [██████████████░░░░░░░░░░░░░░░░░░░░░░] 50-75MB  (Worker C: Medium)
Segment 4: [████████████████████░░░░░░░░░░░░░░░░] 75-100MB (Worker D: Fast)

Worker A completes → Steals from Worker B (largest remaining)
═══════════════════════════════════════════════════════════════════════════

Segment 1: [████████████████████████████████████] 0-25MB   ✓ COMPLETE
Segment 2: [████████████░░░░░░░░░░░░░░░░░░░░░░░░] 25-37MB  (Worker B: continues)
Segment 2': [░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] 37-50MB  (Worker A: new segment)
Segment 3: [██████████████████░░░░░░░░░░░░░░░░░░] 50-75MB  (Worker C: continues)
Segment 4: [████████████████████████████████████] 75-100MB ✓ COMPLETE

Worker D completes → Steals from Worker C
═══════════════════════════════════════════════════════════════════════════

All segments complete → Merge phase
```

### 4.4 Key Constants

```cpp
namespace SegmentConstants {
    constexpr size_t MAX_SEGMENTS = 32;
    constexpr size_t MIN_SEGMENTS = 1;
    constexpr size_t MIN_SEGMENT_SIZE = 1 * 1024 * 1024;      // 1 MB
    constexpr size_t MIN_STEAL_SIZE = 512 * 1024;              // 512 KB
    constexpr size_t CHUNK_SIZE = 64 * 1024;                   // 64 KB per read
    constexpr size_t PROGRESS_UPDATE_INTERVAL_MS = 100;
    constexpr size_t REBALANCE_INTERVAL_MS = 5000;
    constexpr size_t MAX_RETRIES = 5;
    constexpr size_t RETRY_BACKOFF_BASE_MS = 1000;
}
```

---

## 5. Persistence Layer Design

### 5.1 SQLite Schema

```sql
-- Downloads table
CREATE TABLE downloads (
    id              TEXT PRIMARY KEY,
    url             TEXT NOT NULL,
    file_path       TEXT NOT NULL,
    file_name       TEXT NOT NULL,
    total_size      INTEGER NOT NULL,
    downloaded_size INTEGER DEFAULT 0,
    state           INTEGER NOT NULL,
    supports_ranges INTEGER DEFAULT 1,
    created_at      INTEGER NOT NULL,
    updated_at      INTEGER NOT NULL,
    completed_at    INTEGER,
    content_type    TEXT,
    checksum        TEXT,
    error_message   TEXT
);

-- Segments table
CREATE TABLE segments (
    id              INTEGER PRIMARY KEY,
    download_id     TEXT NOT NULL,
    segment_index   INTEGER NOT NULL,
    start_byte      INTEGER NOT NULL,
    end_byte        INTEGER NOT NULL,
    current_byte    INTEGER NOT NULL,
    state           INTEGER NOT NULL,
    checksum        INTEGER,
    temp_file       TEXT,
    retry_count     INTEGER DEFAULT 0,
    last_error      TEXT,
    FOREIGN KEY (download_id) REFERENCES downloads(id) ON DELETE CASCADE
);

-- Settings table
CREATE TABLE settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- Indexes for performance
CREATE INDEX idx_downloads_state ON downloads(state);
CREATE INDEX idx_segments_download ON segments(download_id);
CREATE INDEX idx_segments_state ON segments(download_id, state);

-- Enable WAL mode
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;
```

### 5.2 Recovery Strategy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Crash Recovery Strategy                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  On Application Startup:                                                    │
│  ──────────────────────                                                     │
│                                                                             │
│  1. Open database (WAL mode auto-recovers)                                  │
│  2. Query all downloads with state = Downloading or Probing                 │
│  3. For each interrupted download:                                          │
│     a. Verify temp files exist on disk                                      │
│     b. Validate segment checksums (CRC32)                                   │
│     c. Update current_byte based on actual file sizes                       │
│     d. Set state to Paused                                                  │
│  4. Notify user of recovered downloads                                      │
│                                                                             │
│  Checkpoint Strategy:                                                       │
│  ───────────────────                                                        │
│                                                                             │
│  - Segment progress: Write every 1MB or 5 seconds (whichever first)         │
│  - Download state: Write immediately on state change                        │
│  - WAL checkpoint: Every 30 seconds or 1000 pages                           │
│                                                                             │
│  Corruption Detection:                                                      │
│  ────────────────────                                                       │
│                                                                             │
│  Each segment stores rolling CRC32 checksum.                                │
│  On resume: verify checksum of existing data before continuing.             │
│  If mismatch: re-download affected segment from beginning.                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 6. UI/MVVM Architecture

### 6.1 MVVM Layer Separation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            MVVM Architecture                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  VIEW (QML)                                                                 │
│  ──────────                                                                 │
│  - Pure presentation logic                                                  │
│  - Declarative bindings to ViewModel properties                             │
│  - User interaction handling → ViewModel commands                           │
│  - Animations and visual effects                                            │
│                                                                             │
│  FILES:                                                                     │
│  ├── Main.qml                 (Application root)                            │
│  ├── views/                                                                 │
│  │   ├── DownloadListView.qml                                              │
│  │   ├── AddDownloadDialog.qml                                             │
│  │   └── SettingsPage.qml                                                  │
│  ├── components/                                                            │
│  │   ├── DownloadItemDelegate.qml                                          │
│  │   ├── ProgressBar.qml                                                   │
│  │   ├── SpeedIndicator.qml                                                │
│  │   └── ActionButton.qml                                                  │
│  └── theme/                                                                 │
│      └── Theme.qml            (Singleton with design tokens)               │
│                                                                             │
│  ─────────────────────────────────────────────────────────────────────────  │
│                                                                             │
│  VIEWMODEL (C++ QObject)                                                    │
│  ───────────────────────                                                    │
│  - Exposes data via Q_PROPERTY                                              │
│  - Provides Q_INVOKABLE methods for commands                                │
│  - Transforms Model data for View consumption                               │
│  - Handles validation and business logic                                    │
│                                                                             │
│  CLASSES:                                                                   │
│  ├── DownloadListModel : QAbstractListModel                                │
│  │   - Provides role-based data for ListView                               │
│  │   - Batched updates for performance                                     │
│  │                                                                         │
│  ├── DownloadViewModel : QObject                                           │
│  │   - addDownload(url), pauseDownload(id), etc.                           │
│  │   - globalSpeed, activeCount, queuedCount                               │
│  │                                                                         │
│  └── SettingsViewModel : QObject                                           │
│      - maxConcurrentDownloads, downloadDirectory, etc.                     │
│                                                                             │
│  ─────────────────────────────────────────────────────────────────────────  │
│                                                                             │
│  MODEL (Engine + Persistence)                                               │
│  ────────────────────────────                                               │
│  - Pure data and business logic                                             │
│  - No Qt dependencies (except for signals)                                  │
│  - Thread-safe operations                                                   │
│                                                                             │
│  CLASSES:                                                                   │
│  ├── DownloadManager                                                        │
│  ├── DownloadTask                                                           │
│  ├── SegmentScheduler                                                       │
│  └── PersistenceManager                                                     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Data Flow

```
User clicks "Add Download"
        │
        ▼
┌─────────────────────┐
│ AddDownloadDialog   │  QML
│ url: TextField.text │
└─────────────────────┘
        │ Button.onClicked: viewModel.addDownload(url)
        ▼
┌─────────────────────┐
│ DownloadViewModel   │  C++ QObject
│ Q_INVOKABLE         │
│ addDownload(url)    │
└─────────────────────┘
        │ validate, then call manager
        ▼
┌─────────────────────┐
│ DownloadManager     │  C++ Engine
│ addDownload(url)    │
└─────────────────────┘
        │ creates task, notifies
        ▼
┌─────────────────────┐
│ DownloadListModel   │  C++ QAbstractListModel
│ beginInsertRows()   │
│ updateRole(Progress)│
└─────────────────────┘
        │ dataChanged signal
        ▼
┌─────────────────────┐
│ ListView delegate   │  QML auto-updates
│ property bindings   │
└─────────────────────┘
```

---

## 7. Platform Abstraction

### 7.1 Interface Design

```cpp
class IPlatformIntegration {
public:
    virtual ~IPlatformIntegration() = default;
    
    // Notifications
    virtual void showNotification(const QString& title, 
                                   const QString& message,
                                   NotificationType type) = 0;
    
    // System tray (desktop only)
    virtual void createSystemTray() = 0;
    virtual void updateTrayProgress(double progress) = 0;
    
    // File associations
    virtual void registerFileAssociations() = 0;
    
    // Power management
    virtual void preventSleep(bool prevent) = 0;
    
    // Browser integration
    virtual void installBrowserExtension() = 0;
    
    // Storage
    virtual QString getDefaultDownloadPath() = 0;
    virtual int64_t getAvailableSpace(const QString& path) = 0;
    
    // Factory
    static std::unique_ptr<IPlatformIntegration> create();
};
```

### 7.2 Platform-Specific Implementations

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Platform Implementations                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Windows (WindowsPlatform.cpp)                                              │
│  ─────────────────────────────                                              │
│  - WinToast for modern notifications                                        │
│  - Shell_NotifyIcon for system tray                                         │
│  - Registry for file associations                                           │
│  - SetThreadExecutionState for sleep prevention                             │
│  - Native Message Host for browser extension                                │
│                                                                             │
│  Linux (LinuxPlatform.cpp)                                                  │
│  ───────────────────────────                                                │
│  - libnotify / D-Bus for notifications                                      │
│  - AppIndicator / StatusNotifier for tray                                   │
│  - XDG MIME for file associations                                           │
│  - systemd-inhibit for sleep prevention                                     │
│                                                                             │
│  macOS (MacOSPlatform.mm)                                                   │
│  ─────────────────────────                                                  │
│  - UNUserNotificationCenter for notifications                               │
│  - NSStatusItem for menu bar                                                │
│  - LSSetDefaultHandlerForURLScheme for associations                         │
│  - IOPMAssertionCreateWithName for sleep prevention                         │
│                                                                             │
│  Android (AndroidPlatform.cpp + Java JNI)                                   │
│  ─────────────────────────────────────────                                  │
│  - NotificationManager for notifications                                    │
│  - No system tray (foreground service instead)                              │
│  - Intent filters for file associations                                     │
│  - WakeLock for download continuation                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Appendix A: Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| libcurl over Qt Network | Fine-grained control over connections, proven multi-segment support, better error handling |
| SQLite with WAL | ACID compliance, single-file database, excellent crash recovery, minimal dependencies |
| std::atomic for progress | Lock-free updates from worker threads, no contention for hot paths |
| Work-stealing scheduler | Maximizes bandwidth by keeping all workers busy, adapts to variable segment speeds |
| Qt signals (queued) for UI | Thread-safe UI updates without explicit locking, natural Qt integration |
| 100ms UI update interval | Balance between responsiveness and CPU usage |
| CRC32 for segment checksums | Fast computation, sufficient for corruption detection |

---

## Appendix B: Performance Targets

| Metric | Target |
|--------|--------|
| Max concurrent downloads | 8 (configurable) |
| Max segments per download | 32 |
| Memory per active download | < 10 MB |
| UI thread blocking | < 16ms (60 FPS) |
| Segment checkpoint interval | 1 MB or 5 seconds |
| Startup time (cold) | < 2 seconds |
| Resume time | < 500ms |

