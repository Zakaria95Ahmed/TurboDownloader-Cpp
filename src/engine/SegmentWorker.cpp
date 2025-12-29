/**
 * @file SegmentWorker.cpp
 * @brief Implementation of SegmentWorker - downloads segments using libcurl
 */

#include "openidm/engine/SegmentWorker.h"
#include "openidm/engine/SegmentScheduler.h"
#include "openidm/engine/DownloadTask.h"

#include <curl/curl.h>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

SegmentWorker::SegmentWorker(DownloadTask* task, SegmentScheduler* scheduler, QObject* parent)
    : QObject(parent)
    , m_task(task)
    , m_scheduler(scheduler)
{
    setAutoDelete(false);  // We manage lifecycle manually
    
    // Reserve space for speed samples
    m_speedSamples.reserve(Constants::SPEED_HISTORY_SIZE);
}

SegmentWorker::~SegmentWorker() {
    stop();
    cleanupCurl();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main Worker Loop
// ═══════════════════════════════════════════════════════════════════════════════

void SegmentWorker::run() {
    qDebug() << "SegmentWorker: Starting worker thread";
    
    // Initialize curl for this thread
    if (!initCurl()) {
        m_state.store(State::Error, std::memory_order_release);
        emit finished();
        return;
    }
    
    // Register with scheduler
    m_scheduler->registerWorker(this);
    
    // Main work loop
    while (shouldContinue()) {
        // Wait while paused
        waitWhilePaused();
        
        if (!shouldContinue()) break;
        
        // Try to acquire a segment
        Segment* segment = m_scheduler->acquireSegment(this);
        
        if (!segment) {
            // No work available - wait for more or termination
            if (!m_scheduler->waitForWork(std::chrono::seconds(1))) {
                // Timeout - check if all work is done
                if (m_scheduler->isAllComplete()) {
                    break;
                }
                continue;
            }
            continue;
        }
        
        // Download the segment
        m_state.store(State::Downloading, std::memory_order_release);
        emit stateChanged(State::Downloading);
        
        {
            QMutexLocker locker(&m_segmentMutex);
            m_currentSegment = segment;
        }
        
        m_segmentBytesDownloaded.store(0);
        m_segmentStartTime = std::chrono::system_clock::now();
        
        bool success = downloadSegment(segment);
        
        {
            QMutexLocker locker(&m_segmentMutex);
            m_currentSegment = nullptr;
        }
        
        // Report result to scheduler
        if (success) {
            segment->setState(SegmentState::Completed);
            emit segmentCompleted(segment);
        }
        
        m_scheduler->releaseSegment(this, segment);
        
        if (!success && !shouldContinue()) {
            break;
        }
        
        m_state.store(State::Idle, std::memory_order_release);
        emit stateChanged(State::Idle);
    }
    
    // Cleanup
    m_scheduler->unregisterWorker(this);
    cleanupCurl();
    
    qDebug() << "SegmentWorker: Worker thread finished";
    emit finished();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Control
// ═══════════════════════════════════════════════════════════════════════════════

void SegmentWorker::stop() {
    m_shouldStop.store(true, std::memory_order_release);
    
    // Wake up if paused
    {
        QMutexLocker locker(&m_pauseMutex);
        m_isPaused.store(false, std::memory_order_release);
    }
    m_pauseCondition.wakeAll();
}

void SegmentWorker::pause() {
    m_isPaused.store(true, std::memory_order_release);
    m_state.store(State::Paused, std::memory_order_release);
    emit stateChanged(State::Paused);
}

void SegmentWorker::resume() {
    {
        QMutexLocker locker(&m_pauseMutex);
        m_isPaused.store(false, std::memory_order_release);
    }
    m_pauseCondition.wakeAll();
    
    m_state.store(State::Downloading, std::memory_order_release);
    emit stateChanged(State::Downloading);
}

Segment* SegmentWorker::currentSegment() const {
    QMutexLocker locker(&m_segmentMutex);
    return m_currentSegment;
}

SpeedBps SegmentWorker::currentSpeed() const {
    return m_currentSpeed.load(std::memory_order_relaxed);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Download Implementation
// ═══════════════════════════════════════════════════════════════════════════════

bool SegmentWorker::downloadSegment(Segment* segment) {
    if (!segment || !m_curl) {
        return false;
    }
    
    // Open temp file for this segment
    if (!openTempFile(segment)) {
        DownloadError error;
        error.category = ErrorCategory::FileSystem;
        error.message = QStringLiteral("Failed to open temp file: %1").arg(segment->tempFilePath());
        emit errorOccurred(segment, error);
        return false;
    }
    
    // Configure curl for this segment
    if (!configureCurl(segment)) {
        closeTempFile();
        return false;
    }
    
    qDebug() << "SegmentWorker: Downloading segment" << segment->id()
             << "Range:" << segment->currentByte() << "-" << segment->endByte();
    
    // Perform the download
    CURLcode result = curl_easy_perform(m_curl);
    
    // Close file
    closeTempFile();
    
    // Handle result
    if (result != CURLE_OK) {
        // Check if it was an intentional abort (pause/stop)
        if (result == CURLE_ABORTED_BY_CALLBACK) {
            if (m_shouldStop.load()) {
                return false;
            }
            if (m_isPaused.load()) {
                segment->setState(SegmentState::Paused);
                return false;
            }
        }
        
        // Real error
        DownloadError error = handleCurlError(result, segment);
        segment->setLastError(error.message);
        segment->setState(SegmentState::Failed);
        emit errorOccurred(segment, error);
        return false;
    }
    
    // Verify we downloaded the expected amount
    ByteCount expected = segment->endByte() - segment->startByte() + 1;
    ByteCount actual = segment->downloadedBytes();
    
    if (actual < expected) {
        qWarning() << "SegmentWorker: Segment" << segment->id()
                   << "incomplete. Expected:" << expected << "Got:" << actual;
        segment->setLastError(QStringLiteral("Incomplete download"));
        segment->setState(SegmentState::Failed);
        return false;
    }
    
    qDebug() << "SegmentWorker: Segment" << segment->id() << "completed successfully";
    return true;
}

bool SegmentWorker::initCurl() {
    m_curl = curl_easy_init();
    
    if (!m_curl) {
        qCritical() << "SegmentWorker: Failed to initialize curl";
        return false;
    }
    
    // Set common options
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, "");  // Accept any encoding
    
    // Timeouts
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, 
                     std::chrono::duration_cast<std::chrono::milliseconds>(Constants::CONNECT_TIMEOUT).count());
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 1L);  // 1 byte/sec
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, 60L);  // For 60 seconds
    
    // SSL verification (enable by default for security)
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Use system CA bundle
#ifdef Q_OS_WIN
    // On Windows, use Windows certificate store
    curl_easy_setopt(m_curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
    
    return true;
}

bool SegmentWorker::configureCurl(Segment* segment) {
    if (!m_curl || !segment) {
        return false;
    }
    
    // URL
    QByteArray urlBytes = m_task->url().toUtf8();
    curl_easy_setopt(m_curl, CURLOPT_URL, urlBytes.constData());
    
    // Range header
    QString range = segment->rangeHeader();
    QByteArray rangeBytes = range.toUtf8();
    curl_easy_setopt(m_curl, CURLOPT_RANGE, rangeBytes.constData());
    
    // Callbacks
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
    
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, this);
    
    // User agent
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "OpenIDM/1.0 (https://github.com/openidm)");
    
    return true;
}

void SegmentWorker::cleanupCurl() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
        m_curl = nullptr;
    }
}

bool SegmentWorker::openTempFile(Segment* segment) {
    QString tempPath = segment->tempFilePath();
    
    if (tempPath.isEmpty()) {
        // Generate temp file path
        QString dir = QFileInfo(m_task->filePath()).path();
        tempPath = QStringLiteral("%1/.%2.part%3")
                      .arg(dir)
                      .arg(QFileInfo(m_task->filePath()).fileName())
                      .arg(segment->id());
        segment->setTempFilePath(tempPath);
    }
    
    // Ensure directory exists
    QDir().mkpath(QFileInfo(tempPath).path());
    
    m_tempFile = std::make_unique<QFile>(tempPath);
    
    // Open for append if resuming, otherwise write
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    if (segment->downloadedBytes() > 0) {
        mode |= QIODevice::Append;
    }
    
    if (!m_tempFile->open(mode)) {
        qWarning() << "SegmentWorker: Failed to open temp file:" << tempPath
                   << m_tempFile->errorString();
        m_tempFile.reset();
        return false;
    }
    
    return true;
}

void SegmentWorker::closeTempFile() {
    if (m_tempFile) {
        m_tempFile->flush();
        m_tempFile->close();
        m_tempFile.reset();
    }
}

DownloadError SegmentWorker::handleCurlError(int code, Segment* segment) {
    DownloadError error;
    error.errorCode = code;
    error.timestamp = std::chrono::system_clock::now();
    error.retryCount = segment->retryCount();
    
    const char* errorStr = curl_easy_strerror(static_cast<CURLcode>(code));
    error.details = QString::fromUtf8(errorStr);
    
    switch (code) {
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_RECV_ERROR:
        case CURLE_SEND_ERROR:
            error.category = ErrorCategory::Network;
            error.message = QStringLiteral("Network error: %1").arg(error.details);
            break;
            
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_PEER_FAILED_VERIFICATION:
            error.category = ErrorCategory::SSLError;
            error.message = QStringLiteral("SSL error: %1").arg(error.details);
            break;
            
        case CURLE_HTTP_RETURNED_ERROR:
            error.category = ErrorCategory::ServerError;
            long httpCode = 0;
            curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
            error.errorCode = httpCode;
            error.message = QStringLiteral("HTTP error %1").arg(httpCode);
            break;
            
        default:
            error.category = ErrorCategory::Unknown;
            error.message = QStringLiteral("Download error: %1").arg(error.details);
            break;
    }
    
    return error;
}

void SegmentWorker::updateSpeed(ByteCount bytes) {
    auto now = std::chrono::system_clock::now();
    
    QMutexLocker locker(&m_speedMutex);
    
    m_speedSamples.push_back({bytes, now});
    
    // Remove old samples (older than smoothing window)
    auto cutoff = now - Constants::SPEED_SMOOTHING_WINDOW;
    while (!m_speedSamples.empty() && m_speedSamples.front().time < cutoff) {
        m_speedSamples.erase(m_speedSamples.begin());
    }
    
    // Calculate average speed
    if (m_speedSamples.size() >= 2) {
        ByteCount totalBytes = 0;
        for (const auto& sample : m_speedSamples) {
            totalBytes += sample.bytes;
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            m_speedSamples.back().time - m_speedSamples.front().time
        );
        
        if (duration.count() > 0) {
            SpeedBps speed = static_cast<double>(totalBytes) / (duration.count() / 1000.0);
            m_currentSpeed.store(speed, std::memory_order_relaxed);
        }
    }
}

void SegmentWorker::waitWhilePaused() {
    QMutexLocker locker(&m_pauseMutex);
    while (m_isPaused.load(std::memory_order_acquire) && shouldContinue()) {
        m_pauseCondition.wait(&m_pauseMutex, 100);  // Check every 100ms
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Curl Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

size_t SegmentWorker::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* worker = static_cast<SegmentWorker*>(userdata);
    size_t totalSize = size * nmemb;
    
    if (!worker->m_tempFile || !worker->m_tempFile->isOpen()) {
        return 0;  // Abort transfer
    }
    
    // Write to temp file
    qint64 written = worker->m_tempFile->write(ptr, totalSize);
    
    if (written != static_cast<qint64>(totalSize)) {
        qWarning() << "SegmentWorker: Write failed, expected:" << totalSize
                   << "written:" << written;
        return 0;  // Abort transfer
    }
    
    // Update segment progress
    Segment* segment = worker->currentSegment();
    if (segment) {
        segment->advanceBy(totalSize);
        segment->updateChecksum(ptr, totalSize);
    }
    
    // Update worker statistics
    worker->m_segmentBytesDownloaded.fetch_add(totalSize);
    worker->m_totalBytesDownloaded.fetch_add(totalSize);
    worker->updateSpeed(totalSize);
    
    // Report throughput to scheduler
    worker->m_scheduler->reportThroughput(worker, worker->currentSpeed());
    
    return totalSize;
}

int SegmentWorker::progressCallback(void* clientp, 
                                     double /*dltotal*/, double /*dlnow*/,
                                     double /*ultotal*/, double /*ulnow*/) {
    auto* worker = static_cast<SegmentWorker*>(clientp);
    
    // Check if we should abort
    if (worker->m_shouldStop.load(std::memory_order_acquire)) {
        return 1;  // Non-zero aborts transfer
    }
    
    // Check if paused
    if (worker->m_isPaused.load(std::memory_order_acquire)) {
        return 1;  // Abort to pause
    }
    
    // Emit progress periodically
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - worker->m_lastProgressReport
    );
    
    if (elapsed >= Constants::PROGRESS_UPDATE_INTERVAL) {
        worker->m_lastProgressReport = now;
        emit worker->progressUpdated(
            worker->m_segmentBytesDownloaded.load(),
            worker->currentSpeed()
        );
    }
    
    return 0;  // Continue transfer
}

size_t SegmentWorker::headerCallback(char* buffer, size_t size, size_t nitems, void* /*userdata*/) {
    size_t totalSize = size * nitems;
    
    // Parse headers if needed (e.g., for Content-Range validation)
    // QString header = QString::fromUtf8(buffer, totalSize).trimmed();
    
    return totalSize;
}

} // namespace OpenIDM
