/**
 * @file DownloadTask.cpp
 * @brief Implementation of DownloadTask - manages a single file download
 */

#include "openidm/engine/DownloadTask.h"
#include "openidm/engine/NetworkProbe.h"
#include "openidm/persistence/PersistenceManager.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QThreadPool>
#include <algorithm>
#include <numeric>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

DownloadTask::DownloadTask(const QUrl& url, const QString& destPath, QObject* parent)
    : QObject(parent)
    , m_id(QUuid::createUuid())
    , m_url(url)
    , m_scheduler(std::make_unique<SegmentScheduler>(this, this))
    , m_threadPool(QThreadPool::globalInstance())
    , m_progressTimer(new QTimer(this))
{
    // Extract filename from URL
    m_fileName = url.fileName();
    if (m_fileName.isEmpty()) {
        m_fileName = QStringLiteral("download");
    }
    
    // Setup destination path
    if (destPath.isEmpty()) {
        m_destDir = QDir::homePath() + QStringLiteral("/Downloads");
    } else {
        QFileInfo fi(destPath);
        if (fi.isDir()) {
            m_destDir = destPath;
        } else {
            m_destDir = fi.path();
            m_fileName = fi.fileName();
        }
    }
    
    m_filePath = m_destDir + QDir::separator() + m_fileName;
    
    // Connect scheduler signals
    connect(m_scheduler.get(), &SegmentScheduler::segmentCompleted,
            this, &DownloadTask::onSegmentCompleted);
    connect(m_scheduler.get(), &SegmentScheduler::segmentFailed,
            this, &DownloadTask::onSegmentFailed);
    connect(m_scheduler.get(), &SegmentScheduler::allSegmentsCompleted,
            this, &DownloadTask::onAllSegmentsCompleted);
    
    // Progress timer
    connect(m_progressTimer, &QTimer::timeout, this, &DownloadTask::onProgressTimer);
    m_progressTimer->setInterval(
        std::chrono::duration_cast<std::chrono::milliseconds>(Constants::PROGRESS_UPDATE_INTERVAL).count()
    );
    
    qDebug() << "DownloadTask: Created task" << m_id.toString() << "for" << m_url.toString();
}

DownloadTask::DownloadTask(const TaskId& id, const QUrl& url, const QString& destPath, QObject* parent)
    : DownloadTask(url, destPath, parent)
{
    m_id = id;
}

DownloadTask::~DownloadTask() {
    stopWorkers();
    cleanupTempFiles();
}

// ═══════════════════════════════════════════════════════════════════════════════
// File Information
// ═══════════════════════════════════════════════════════════════════════════════

QString DownloadTask::directory() const {
    return QFileInfo(m_filePath).path();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Progress
// ═══════════════════════════════════════════════════════════════════════════════

double DownloadTask::progress() const {
    ByteCount total = totalSize();
    if (total <= 0) {
        return 0.0;
    }
    
    ByteCount downloaded = downloadedSize();
    return (static_cast<double>(downloaded) / total) * 100.0;
}

SpeedBps DownloadTask::averageSpeed() const {
    if (m_speedHistory.empty()) {
        return 0.0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);
    
    if (elapsed.count() <= 0) {
        return speed();
    }
    
    return static_cast<double>(downloadedSize()) / elapsed.count();
}

Duration DownloadTask::remainingTime() const {
    SpeedBps currentSpeed = speed();
    
    if (currentSpeed <= 0 || totalSize() <= 0) {
        return Duration{-1};
    }
    
    ByteCount remaining = totalSize() - downloadedSize();
    double seconds = static_cast<double>(remaining) / currentSpeed;
    
    return Duration{static_cast<int64_t>(seconds * 1000)};
}

int DownloadTask::activeSegments() const {
    return static_cast<int>(m_scheduler->activeWorkerCount());
}

int DownloadTask::totalSegments() const {
    return static_cast<int>(m_scheduler->segmentCount());
}

int DownloadTask::completedSegments() const {
    return static_cast<int>(m_scheduler->segmentsInState(SegmentState::Completed).size());
}

DownloadProgress DownloadTask::progressInfo() const {
    DownloadProgress info;
    info.downloadedBytes = downloadedSize();
    info.totalBytes = totalSize();
    info.currentSpeed = speed();
    info.averageSpeed = averageSpeed();
    info.remainingTime = remainingTime();
    info.progressPercent = progress();
    info.activeSegments = activeSegments();
    info.completedSegments = completedSegments();
    info.totalSegments = totalSegments();
    return info;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Priority
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadTask::setPriority(Priority priority) {
    if (m_priority != priority) {
        m_priority = priority;
        emit needsPersistence();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Actions
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadTask::start() {
    if (state() == DownloadState::Downloading) {
        return;  // Already running
    }
    
    if (state() == DownloadState::Completed) {
        return;  // Already done
    }
    
    qDebug() << "DownloadTask: Starting download" << m_id.toString();
    
    // Record start time
    m_startTime = std::chrono::system_clock::now();
    
    // If we have server capabilities, skip probing
    if (m_capabilities.isValid()) {
        initializeSegments();
        startWorkers();
    } else {
        // Probe server first
        probeServer();
    }
}

void DownloadTask::pause() {
    if (state() != DownloadState::Downloading) {
        return;
    }
    
    qDebug() << "DownloadTask: Pausing download" << m_id.toString();
    
    setState(DownloadState::Paused);
    
    // Pause all workers
    for (auto& worker : m_workers) {
        worker->pause();
    }
    
    m_scheduler->pauseAll();
    m_progressTimer->stop();
    
    // Record elapsed time
    auto now = std::chrono::system_clock::now();
    m_elapsedTime += std::chrono::duration_cast<Duration>(now - m_startTime);
    
    emit needsPersistence();
}

void DownloadTask::resume() {
    if (state() != DownloadState::Paused) {
        return;
    }
    
    qDebug() << "DownloadTask: Resuming download" << m_id.toString();
    
    setState(DownloadState::Downloading);
    
    m_startTime = std::chrono::system_clock::now();
    
    m_scheduler->resumeAll();
    
    // Resume workers
    for (auto& worker : m_workers) {
        worker->resume();
    }
    
    m_progressTimer->start();
}

void DownloadTask::cancel() {
    qDebug() << "DownloadTask: Cancelling download" << m_id.toString();
    
    setState(DownloadState::Failed);
    
    m_lastError.category = ErrorCategory::Cancelled;
    m_lastError.message = QStringLiteral("Download cancelled by user");
    
    stopWorkers();
    m_scheduler->cancelAll();
    m_progressTimer->stop();
    
    cleanupTempFiles();
    
    emit errorChanged();
}

void DownloadTask::retry() {
    if (state() != DownloadState::Failed) {
        return;
    }
    
    qDebug() << "DownloadTask: Retrying download" << m_id.toString();
    
    // Reset error
    m_lastError = DownloadError{};
    
    // Reset scheduler
    m_scheduler->reset();
    
    // Start fresh
    setState(DownloadState::Queued);
    start();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Slots
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadTask::onProbeCompleted(const ServerCapabilities& caps) {
    qDebug() << "DownloadTask: Probe completed. Size:" << caps.contentLength
             << "Ranges:" << caps.supportsRanges;
    
    m_capabilities = caps;
    
    // Update file info from server
    if (!caps.fileName.isEmpty()) {
        m_fileName = caps.fileName;
        m_filePath = m_destDir + QDir::separator() + m_fileName;
        emit fileNameChanged();
        emit filePathChanged();
    }
    
    if (caps.contentLength > 0) {
        m_totalSize.store(caps.contentLength, std::memory_order_relaxed);
        emit totalSizeChanged();
    }
    
    initializeSegments();
    startWorkers();
}

void DownloadTask::onProbeFailed(const DownloadError& error) {
    qWarning() << "DownloadTask: Probe failed:" << error.message;
    
    setError(error);
    setState(DownloadState::Failed);
    emit failed(error);
}

void DownloadTask::onSegmentCompleted(SegmentId id) {
    qDebug() << "DownloadTask: Segment" << id << "completed";
    
    updateStatistics();
    emit progressChanged();
    emit needsPersistence();
}

void DownloadTask::onSegmentFailed(SegmentId id, const QString& error) {
    qWarning() << "DownloadTask: Segment" << id << "failed:" << error;
    
    // Check if all retries exhausted
    if (m_scheduler->hasFailed()) {
        DownloadError err;
        err.category = ErrorCategory::Network;
        err.message = error;
        setError(err);
        setState(DownloadState::Failed);
        emit failed(err);
    }
}

void DownloadTask::onAllSegmentsCompleted() {
    qDebug() << "DownloadTask: All segments completed, starting merge";
    
    setState(DownloadState::Merging);
    m_progressTimer->stop();
    
    // Merge segment files
    if (!mergeSegments()) {
        DownloadError error;
        error.category = ErrorCategory::FileSystem;
        error.message = QStringLiteral("Failed to merge segment files");
        setError(error);
        setState(DownloadState::Failed);
        emit failed(error);
        return;
    }
    
    // Verify file (optional)
    setState(DownloadState::Verifying);
    if (!verifyFile()) {
        qWarning() << "DownloadTask: File verification failed (non-fatal)";
    }
    
    // Cleanup temp files
    cleanupTempFiles();
    
    // Record completion
    m_endTime = std::chrono::system_clock::now();
    setState(DownloadState::Completed);
    
    emit completed();
    emit needsPersistence();
}

void DownloadTask::onProgressTimer() {
    updateStatistics();
    emit progressChanged();
    emit speedChanged();
    
    // Check if persistence needed
    ByteCount downloaded = downloadedSize();
    if (downloaded - m_lastPersistedBytes >= Constants::PERSISTENCE_CHECKPOINT_BYTES) {
        m_lastPersistedBytes = downloaded;
        emit needsPersistence();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Methods
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadTask::probeServer() {
    setState(DownloadState::Probing);
    
    m_probe = std::make_unique<NetworkProbe>(this);
    
    connect(m_probe.get(), &NetworkProbe::completed,
            this, &DownloadTask::onProbeCompleted);
    connect(m_probe.get(), &NetworkProbe::failed,
            this, &DownloadTask::onProbeFailed);
    
    m_probe->probe(m_url);
}

void DownloadTask::initializeSegments() {
    ByteCount fileSize = totalSize();
    
    size_t segmentCount;
    if (!m_capabilities.supportsRanges || fileSize <= 0) {
        // Single segment for non-resumable downloads
        segmentCount = 1;
        qDebug() << "DownloadTask: Using single segment (no range support)";
    } else {
        segmentCount = SegmentScheduler::calculateOptimalSegmentCount(fileSize);
        qDebug() << "DownloadTask: Using" << segmentCount << "segments";
    }
    
    m_scheduler->initializeSegments(fileSize, segmentCount);
}

void DownloadTask::startWorkers() {
    setState(DownloadState::Downloading);
    
    // Calculate number of workers
    size_t segmentCount = m_scheduler->segmentCount();
    size_t workerCount = std::min(segmentCount, 
                                  static_cast<size_t>(m_threadPool->maxThreadCount()));
    
    // Limit to MAX_SEGMENTS
    workerCount = std::min(workerCount, static_cast<size_t>(Constants::MAX_SEGMENTS));
    
    qDebug() << "DownloadTask: Starting" << workerCount << "workers";
    
    // Create and start workers
    m_workers.clear();
    m_workers.reserve(workerCount);
    
    for (size_t i = 0; i < workerCount; ++i) {
        auto worker = std::make_unique<SegmentWorker>(this, m_scheduler.get(), this);
        
        // Connect worker signals
        connect(worker.get(), &SegmentWorker::finished, this, [this, w = worker.get()]() {
            qDebug() << "DownloadTask: Worker finished";
        });
        
        // Start worker
        m_threadPool->start(worker.get());
        m_workers.push_back(std::move(worker));
    }
    
    // Enable scheduler rebalancing
    m_scheduler->setAutoRebalance(true);
    
    // Start progress timer
    m_progressTimer->start();
}

void DownloadTask::stopWorkers() {
    // Stop all workers
    for (auto& worker : m_workers) {
        worker->stop();
    }
    
    // Wait for workers to finish (with timeout)
    // Note: In a real implementation, we might want to use QThreadPool::waitForDone
    
    m_workers.clear();
}

bool DownloadTask::mergeSegments() {
    qDebug() << "DownloadTask: Merging segments to" << m_filePath;
    
    // Ensure destination directory exists
    QDir().mkpath(QFileInfo(m_filePath).path());
    
    QFile outputFile(m_filePath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        qWarning() << "DownloadTask: Failed to open output file:" << outputFile.errorString();
        return false;
    }
    
    // Get all segments in order
    auto segments = m_scheduler->allSegments();
    std::sort(segments.begin(), segments.end(), [](Segment* a, Segment* b) {
        return a->startByte() < b->startByte();
    });
    
    // Merge each segment
    for (Segment* segment : segments) {
        QString tempPath = segment->tempFilePath();
        QFile tempFile(tempPath);
        
        if (!tempFile.open(QIODevice::ReadOnly)) {
            qWarning() << "DownloadTask: Failed to open temp file:" << tempPath;
            outputFile.close();
            QFile::remove(m_filePath);
            return false;
        }
        
        // Copy in chunks
        constexpr qint64 BUFFER_SIZE = Constants::FILE_BUFFER_SIZE;
        char buffer[BUFFER_SIZE];
        
        while (!tempFile.atEnd()) {
            qint64 read = tempFile.read(buffer, BUFFER_SIZE);
            if (read <= 0) break;
            
            qint64 written = outputFile.write(buffer, read);
            if (written != read) {
                qWarning() << "DownloadTask: Write error during merge";
                tempFile.close();
                outputFile.close();
                QFile::remove(m_filePath);
                return false;
            }
        }
        
        tempFile.close();
    }
    
    outputFile.close();
    
    qDebug() << "DownloadTask: Merge completed successfully";
    return true;
}

bool DownloadTask::verifyFile() {
    // Basic verification: check file size
    QFileInfo fi(m_filePath);
    
    if (!fi.exists()) {
        return false;
    }
    
    ByteCount expectedSize = totalSize();
    if (expectedSize > 0 && fi.size() != expectedSize) {
        qWarning() << "DownloadTask: File size mismatch. Expected:" << expectedSize
                   << "Actual:" << fi.size();
        return false;
    }
    
    // TODO: Full checksum verification if server provided one
    
    return true;
}

void DownloadTask::cleanupTempFiles() {
    auto segments = m_scheduler->allSegments();
    
    for (Segment* segment : segments) {
        QString tempPath = segment->tempFilePath();
        if (!tempPath.isEmpty() && QFile::exists(tempPath)) {
            QFile::remove(tempPath);
        }
    }
}

void DownloadTask::setState(DownloadState newState) {
    DownloadState oldState = m_state.exchange(newState, std::memory_order_acq_rel);
    
    if (oldState != newState) {
        qDebug() << "DownloadTask:" << m_id.toString()
                 << "state changed from" << downloadStateToString(oldState)
                 << "to" << downloadStateToString(newState);
        emit stateChanged(newState);
    }
}

void DownloadTask::setError(const DownloadError& error) {
    m_lastError = error;
    emit errorChanged();
}

void DownloadTask::updateStatistics() {
    // Aggregate downloaded bytes from scheduler
    ByteCount downloaded = m_scheduler->totalDownloadedBytes();
    m_downloadedBytes.store(downloaded, std::memory_order_relaxed);
    
    // Aggregate speed from scheduler
    SpeedBps speed = m_scheduler->totalThroughput();
    m_currentSpeed.store(speed, std::memory_order_relaxed);
    
    // Record speed history for ETA calculation
    auto now = std::chrono::system_clock::now();
    m_speedHistory.emplace_back(now, downloaded);
    
    // Keep only recent history
    auto cutoff = now - Constants::SPEED_SMOOTHING_WINDOW;
    while (!m_speedHistory.empty() && m_speedHistory.front().first < cutoff) {
        m_speedHistory.erase(m_speedHistory.begin());
    }
}

QString DownloadTask::tempFilePath(SegmentId segmentId) const {
    return QStringLiteral("%1/.%2.part%3")
              .arg(m_destDir)
              .arg(m_fileName)
              .arg(segmentId);
}

} // namespace OpenIDM
