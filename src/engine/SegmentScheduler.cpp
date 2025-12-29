/**
 * @file SegmentScheduler.cpp
 * @brief Implementation of the work-stealing segment scheduler
 */

#include "openidm/engine/SegmentScheduler.h"
#include "openidm/engine/SegmentWorker.h"
#include "openidm/engine/DownloadTask.h"

#include <algorithm>
#include <chrono>
#include <QDebug>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

SegmentScheduler::SegmentScheduler(DownloadTask* task, QObject* parent)
    : QObject(parent)
    , m_task(task)
    , m_rebalanceTimer(new QTimer(this))
{
    connect(m_rebalanceTimer, &QTimer::timeout, this, &SegmentScheduler::onRebalanceTimer);
}

SegmentScheduler::~SegmentScheduler() {
    cancelAll();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<Segment*> SegmentScheduler::initializeSegments(ByteCount totalSize, size_t segmentCount) {
    std::unique_lock lock(m_mutex);
    
    // Clear any existing segments
    m_segments.clear();
    m_pendingQueue.clear();
    m_activeSegments.clear();
    m_completedSegments.clear();
    m_failedSegments.clear();
    m_nextSegmentId.store(0);
    
    // Clamp segment count
    segmentCount = std::clamp(segmentCount, 
                              static_cast<size_t>(Constants::MIN_SEGMENTS),
                              static_cast<size_t>(Constants::MAX_SEGMENTS));
    
    // Calculate segment size
    ByteCount segmentSize = totalSize / segmentCount;
    ByteCount remainder = totalSize % segmentCount;
    
    std::vector<Segment*> result;
    result.reserve(segmentCount);
    
    ByteOffset currentStart = 0;
    
    for (size_t i = 0; i < segmentCount; ++i) {
        // Add remainder bytes to the last segment
        ByteCount thisSize = segmentSize;
        if (i == segmentCount - 1) {
            thisSize += remainder;
        }
        
        ByteOffset startByte = currentStart;
        ByteOffset endByte = currentStart + thisSize - 1;
        
        auto segment = std::make_unique<Segment>(nextSegmentId(), startByte, endByte);
        Segment* ptr = segment.get();
        
        m_segments.push_back(std::move(segment));
        m_pendingQueue.push_back(ptr);
        result.push_back(ptr);
        
        currentStart = endByte + 1;
    }
    
    qDebug() << "SegmentScheduler: Initialized" << segmentCount << "segments for" 
             << totalSize << "bytes";
    
    return result;
}

void SegmentScheduler::restoreSegments(const std::vector<Segment::Snapshot>& snapshots) {
    std::unique_lock lock(m_mutex);
    
    m_segments.clear();
    m_pendingQueue.clear();
    m_activeSegments.clear();
    m_completedSegments.clear();
    m_failedSegments.clear();
    
    SegmentId maxId = 0;
    
    for (const auto& snap : snapshots) {
        auto segment = std::make_unique<Segment>();
        segment->restore(snap);
        
        maxId = std::max(maxId, snap.id);
        
        Segment* ptr = segment.get();
        
        // Place in appropriate collection based on state
        switch (snap.state) {
            case SegmentState::Pending:
            case SegmentState::Paused:
                m_pendingQueue.push_back(ptr);
                break;
            case SegmentState::Completed:
                m_completedSegments.insert(ptr);
                break;
            case SegmentState::Failed:
                m_failedSegments.insert(ptr);
                break;
            default:
                // Active/Stolen segments should be treated as pending on restore
                segment->setState(SegmentState::Pending);
                m_pendingQueue.push_back(ptr);
                break;
        }
        
        m_segments.push_back(std::move(segment));
    }
    
    m_nextSegmentId.store(maxId + 1);
    
    qDebug() << "SegmentScheduler: Restored" << snapshots.size() << "segments,"
             << "pending:" << m_pendingQueue.size()
             << "completed:" << m_completedSegments.size()
             << "failed:" << m_failedSegments.size();
}

size_t SegmentScheduler::calculateOptimalSegmentCount(ByteCount totalSize) {
    if (totalSize <= 0) {
        return Constants::MIN_SEGMENTS;
    }
    
    // Calculate based on minimum segment size
    size_t bySize = totalSize / Constants::MIN_SEGMENT_SIZE;
    
    // Clamp to allowed range
    return std::clamp(bySize,
                      static_cast<size_t>(Constants::MIN_SEGMENTS),
                      static_cast<size_t>(Constants::MAX_SEGMENTS));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Segment Access
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<Segment*> SegmentScheduler::allSegments() const {
    std::shared_lock lock(m_mutex);
    
    std::vector<Segment*> result;
    result.reserve(m_segments.size());
    
    for (const auto& seg : m_segments) {
        result.push_back(seg.get());
    }
    
    return result;
}

Segment* SegmentScheduler::segment(SegmentId id) const {
    std::shared_lock lock(m_mutex);
    
    for (const auto& seg : m_segments) {
        if (seg->id() == id) {
            return seg.get();
        }
    }
    
    return nullptr;
}

size_t SegmentScheduler::segmentCount() const {
    std::shared_lock lock(m_mutex);
    return m_segments.size();
}

std::vector<Segment*> SegmentScheduler::segmentsInState(SegmentState state) const {
    std::shared_lock lock(m_mutex);
    
    std::vector<Segment*> result;
    
    for (const auto& seg : m_segments) {
        if (seg->state() == state) {
            result.push_back(seg.get());
        }
    }
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Work Distribution - Core Work-Stealing Algorithm
// ═══════════════════════════════════════════════════════════════════════════════

Segment* SegmentScheduler::acquireSegment(SegmentWorker* worker) {
    std::unique_lock lock(m_mutex);
    
    if (m_paused || m_cancelled) {
        return nullptr;
    }
    
    // First, try to get from pending queue
    if (!m_pendingQueue.empty()) {
        Segment* segment = m_pendingQueue.front();
        m_pendingQueue.pop_front();
        
        segment->setState(SegmentState::Active);
        m_activeSegments.insert(segment);
        m_workerAssignments[worker] = segment;
        
        qDebug() << "SegmentScheduler: Worker acquired segment" << segment->id()
                 << "from pending queue. Range:" << segment->startByte() << "-" << segment->endByte();
        
        return segment;
    }
    
    // No pending work - try work stealing
    lock.unlock();  // Release lock for stealing (will re-acquire internally)
    return stealWork(worker);
}

void SegmentScheduler::releaseSegment(SegmentWorker* worker, Segment* segment) {
    std::unique_lock lock(m_mutex);
    
    if (!segment) return;
    
    m_activeSegments.erase(segment);
    m_workerAssignments.erase(worker);
    
    // Place back in appropriate collection based on state
    switch (segment->state()) {
        case SegmentState::Completed:
            m_completedSegments.insert(segment);
            lock.unlock();
            emit segmentCompleted(segment->id());
            checkAllComplete();
            break;
            
        case SegmentState::Failed:
            if (segment->canRetry()) {
                segment->setState(SegmentState::Pending);
                m_pendingQueue.push_back(segment);
                m_workCondition.notify_one();
            } else {
                m_failedSegments.insert(segment);
                lock.unlock();
                emit segmentFailed(segment->id(), segment->lastError());
            }
            break;
            
        case SegmentState::Paused:
            m_pendingQueue.push_front(segment);  // High priority for resume
            break;
            
        default:
            // Unexpected state - treat as pending
            segment->setState(SegmentState::Pending);
            m_pendingQueue.push_back(segment);
            break;
    }
}

Segment* SegmentScheduler::stealWork(SegmentWorker* worker) {
    std::unique_lock lock(m_mutex);
    
    if (m_paused || m_cancelled) {
        return nullptr;
    }
    
    // Find the segment with the most remaining bytes
    Segment* largest = findLargestActiveSegment();
    
    if (!largest || !largest->isSplittable()) {
        return nullptr;
    }
    
    // Split the segment
    SegmentId newId = nextSegmentId();
    auto newSegment = largest->split(newId);
    
    if (!newSegment) {
        return nullptr;
    }
    
    qDebug() << "SegmentScheduler: Work stealing - split segment" << largest->id()
             << "created segment" << newId
             << "Range:" << newSegment->startByte() << "-" << newSegment->endByte();
    
    Segment* ptr = newSegment.get();
    ptr->setState(SegmentState::Active);
    
    m_segments.push_back(std::move(newSegment));
    m_activeSegments.insert(ptr);
    m_workerAssignments[worker] = ptr;
    
    lock.unlock();
    emit segmentAdded(newId);
    
    return ptr;
}

void SegmentScheduler::markCompleted(Segment* segment) {
    if (!segment) return;
    
    segment->setState(SegmentState::Completed);
    
    std::unique_lock lock(m_mutex);
    m_activeSegments.erase(segment);
    m_completedSegments.insert(segment);
    lock.unlock();
    
    emit segmentCompleted(segment->id());
    checkAllComplete();
}

void SegmentScheduler::markFailed(Segment* segment, const QString& error) {
    if (!segment) return;
    
    segment->setLastError(error);
    segment->incrementRetry();
    
    std::unique_lock lock(m_mutex);
    m_activeSegments.erase(segment);
    
    if (segment->canRetry()) {
        segment->setState(SegmentState::Pending);
        m_pendingQueue.push_back(segment);
        m_workCondition.notify_one();
    } else {
        segment->setState(SegmentState::Failed);
        m_failedSegments.insert(segment);
        lock.unlock();
        emit segmentFailed(segment->id(), error);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Worker Management
// ═══════════════════════════════════════════════════════════════════════════════

void SegmentScheduler::registerWorker(SegmentWorker* worker) {
    std::unique_lock lock(m_mutex);
    m_workers.insert(worker);
    m_workerStats[worker] = WorkerStats{worker, nullptr, 0.0, 0, Timestamp{}};
}

void SegmentScheduler::unregisterWorker(SegmentWorker* worker) {
    std::unique_lock lock(m_mutex);
    m_workers.erase(worker);
    m_workerAssignments.erase(worker);
    m_workerStats.erase(worker);
}

size_t SegmentScheduler::activeWorkerCount() const {
    std::shared_lock lock(m_mutex);
    return m_activeSegments.size();
}

void SegmentScheduler::notifyWorkerIdle(SegmentWorker* /*worker*/) {
    // Worker will call acquireSegment or waitForWork
}

void SegmentScheduler::wakeAllWorkers() {
    m_workCondition.notify_all();
}

bool SegmentScheduler::waitForWork(Duration timeout) {
    std::unique_lock lock(m_mutex);
    
    // Check if there's work available
    if (!m_pendingQueue.empty() || !m_activeSegments.empty()) {
        return true;
    }
    
    // Wait for work or timeout
    return m_workCondition.wait_for(lock, timeout, [this]() {
        return !m_pendingQueue.empty() || m_cancelled || m_paused;
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// Throughput Monitoring
// ═══════════════════════════════════════════════════════════════════════════════

void SegmentScheduler::reportThroughput(SegmentWorker* worker, SpeedBps bytesPerSecond) {
    std::unique_lock lock(m_mutex);
    
    auto it = m_workerStats.find(worker);
    if (it != m_workerStats.end()) {
        it->second.throughput = bytesPerSecond;
        it->second.lastUpdate = std::chrono::system_clock::now();
    }
}

SpeedBps SegmentScheduler::totalThroughput() const {
    std::shared_lock lock(m_mutex);
    
    SpeedBps total = 0.0;
    for (const auto& [worker, stats] : m_workerStats) {
        total += stats.throughput;
    }
    
    return total;
}

std::vector<SegmentScheduler::WorkerStats> SegmentScheduler::workerStats() const {
    std::shared_lock lock(m_mutex);
    
    std::vector<WorkerStats> result;
    result.reserve(m_workerStats.size());
    
    for (const auto& [worker, stats] : m_workerStats) {
        result.push_back(stats);
    }
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Rebalancing
// ═══════════════════════════════════════════════════════════════════════════════

void SegmentScheduler::rebalanceSegments() {
    std::unique_lock lock(m_mutex);
    
    if (m_paused || m_cancelled || m_activeSegments.empty()) {
        return;
    }
    
    // Calculate average throughput
    SpeedBps avgThroughput = 0.0;
    int activeCount = 0;
    
    for (const auto& [worker, stats] : m_workerStats) {
        if (stats.throughput > 0) {
            avgThroughput += stats.throughput;
            ++activeCount;
        }
    }
    
    if (activeCount > 0) {
        avgThroughput /= activeCount;
    }
    
    // Find segments that are significantly slower than average
    const double SLOW_THRESHOLD = 0.5;  // 50% of average
    int splitCount = 0;
    
    for (auto* segment : m_activeSegments) {
        // Find the worker for this segment
        SegmentWorker* worker = nullptr;
        for (const auto& [w, s] : m_workerAssignments) {
            if (s == segment) {
                worker = w;
                break;
            }
        }
        
        if (!worker) continue;
        
        auto statsIt = m_workerStats.find(worker);
        if (statsIt == m_workerStats.end()) continue;
        
        // Check if this worker is slow and segment is splittable
        if (statsIt->second.throughput < avgThroughput * SLOW_THRESHOLD &&
            segment->isSplittable(Constants::MIN_STEAL_SIZE * 2)) {
            
            // Split and add to pending queue
            SegmentId newId = nextSegmentId();
            auto newSegment = segment->split(newId);
            
            if (newSegment) {
                Segment* ptr = newSegment.get();
                m_segments.push_back(std::move(newSegment));
                m_pendingQueue.push_back(ptr);
                ++splitCount;
                
                qDebug() << "SegmentScheduler: Rebalance split segment" << segment->id()
                         << "due to slow throughput (" << statsIt->second.throughput << "bps)";
            }
        }
    }
    
    if (splitCount > 0) {
        m_workCondition.notify_all();
        lock.unlock();
        emit rebalanced(splitCount);
    }
}

void SegmentScheduler::setAutoRebalance(bool enabled, Duration interval) {
    m_autoRebalance = enabled;
    
    if (enabled) {
        m_rebalanceTimer->setInterval(std::chrono::duration_cast<std::chrono::milliseconds>(interval).count());
        m_rebalanceTimer->start();
    } else {
        m_rebalanceTimer->stop();
    }
}

void SegmentScheduler::onRebalanceTimer() {
    rebalanceSegments();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Progress
// ═══════════════════════════════════════════════════════════════════════════════

ByteCount SegmentScheduler::totalDownloadedBytes() const {
    std::shared_lock lock(m_mutex);
    
    ByteCount total = 0;
    for (const auto& seg : m_segments) {
        total += seg->downloadedBytes();
    }
    
    return total;
}

bool SegmentScheduler::isAllComplete() const {
    std::shared_lock lock(m_mutex);
    return m_pendingQueue.empty() && m_activeSegments.empty() && m_failedSegments.empty();
}

bool SegmentScheduler::hasFailed() const {
    std::shared_lock lock(m_mutex);
    return !m_failedSegments.empty();
}

void SegmentScheduler::checkAllComplete() {
    if (isAllComplete()) {
        emit allSegmentsCompleted();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Control
// ═══════════════════════════════════════════════════════════════════════════════

void SegmentScheduler::pauseAll() {
    std::unique_lock lock(m_mutex);
    m_paused = true;
    
    // Move active segments to pending
    for (auto* segment : m_activeSegments) {
        segment->setState(SegmentState::Paused);
        m_pendingQueue.push_front(segment);
    }
    m_activeSegments.clear();
    m_workerAssignments.clear();
    
    m_workCondition.notify_all();
    
    if (m_rebalanceTimer->isActive()) {
        m_rebalanceTimer->stop();
    }
}

void SegmentScheduler::resumeAll() {
    std::unique_lock lock(m_mutex);
    m_paused = false;
    
    // Change paused segments back to pending
    for (auto& seg : m_segments) {
        if (seg->state() == SegmentState::Paused) {
            seg->setState(SegmentState::Pending);
        }
    }
    
    m_workCondition.notify_all();
    
    if (m_autoRebalance) {
        m_rebalanceTimer->start();
    }
}

void SegmentScheduler::cancelAll() {
    std::unique_lock lock(m_mutex);
    m_cancelled = true;
    m_paused = false;
    
    m_pendingQueue.clear();
    m_activeSegments.clear();
    m_workerAssignments.clear();
    
    m_workCondition.notify_all();
    m_rebalanceTimer->stop();
}

void SegmentScheduler::reset() {
    std::unique_lock lock(m_mutex);
    
    m_segments.clear();
    m_pendingQueue.clear();
    m_activeSegments.clear();
    m_completedSegments.clear();
    m_failedSegments.clear();
    m_workerAssignments.clear();
    m_workerStats.clear();
    m_nextSegmentId.store(0);
    m_paused = false;
    m_cancelled = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════════════

Segment* SegmentScheduler::findLargestActiveSegment() const {
    // Note: Caller must hold lock
    
    Segment* largest = nullptr;
    ByteCount maxRemaining = 0;
    
    for (auto* segment : m_activeSegments) {
        ByteCount remaining = segment->remainingBytes();
        if (remaining > maxRemaining && segment->isSplittable()) {
            maxRemaining = remaining;
            largest = segment;
        }
    }
    
    return largest;
}

Segment* SegmentScheduler::createNewSegment(ByteOffset start, ByteOffset end) {
    auto segment = std::make_unique<Segment>(nextSegmentId(), start, end);
    Segment* ptr = segment.get();
    m_segments.push_back(std::move(segment));
    return ptr;
}

void SegmentScheduler::scheduleSegment(Segment* segment) {
    std::unique_lock lock(m_mutex);
    segment->setState(SegmentState::Pending);
    m_pendingQueue.push_back(segment);
    m_workCondition.notify_one();
}

SegmentId SegmentScheduler::nextSegmentId() {
    return m_nextSegmentId.fetch_add(1);
}

} // namespace OpenIDM
