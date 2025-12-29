/**
 * @file DownloadManager.cpp
 * @brief Implementation of DownloadManager - central download coordination
 */

#include "openidm/engine/DownloadManager.h"
#include "openidm/persistence/PersistenceManager.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Singleton Management
// ═══════════════════════════════════════════════════════════════════════════════

std::unique_ptr<DownloadManager> DownloadManager::s_instance;
bool DownloadManager::s_initialized = false;

DownloadManager& DownloadManager::instance() {
    if (!s_instance) {
        throw std::runtime_error("DownloadManager not initialized. Call initialize() first.");
    }
    return *s_instance;
}

bool DownloadManager::initialize(QObject* parent) {
    if (s_initialized) {
        return true;
    }
    
    qDebug() << "DownloadManager: Initializing...";
    
    s_instance = std::unique_ptr<DownloadManager>(new DownloadManager(parent));
    
    // Initialize persistence
    s_instance->m_persistence = std::make_unique<PersistenceManager>();
    if (!s_instance->m_persistence->initialize()) {
        qCritical() << "DownloadManager: Failed to initialize persistence";
        s_instance.reset();
        return false;
    }
    
    // Load saved state
    s_instance->loadState();
    
    s_initialized = true;
    qDebug() << "DownloadManager: Initialized successfully";
    
    return true;
}

void DownloadManager::shutdown() {
    if (!s_instance) {
        return;
    }
    
    qDebug() << "DownloadManager: Shutting down...";
    
    // Pause all active downloads
    s_instance->pauseAll();
    
    // Save state
    s_instance->saveState();
    
    // Clean up
    s_instance.reset();
    s_initialized = false;
    
    qDebug() << "DownloadManager: Shutdown complete";
}

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
    , m_speedTimer(new QTimer(this))
    , m_queueTimer(new QTimer(this))
{
    // Set default download directory
    m_defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (m_defaultDir.isEmpty()) {
        m_defaultDir = QDir::homePath() + QStringLiteral("/Downloads");
    }
    QDir().mkpath(m_defaultDir);
    
    // Speed update timer
    connect(m_speedTimer, &QTimer::timeout, this, &DownloadManager::onSpeedUpdateTimer);
    m_speedTimer->setInterval(1000);  // 1 second
    m_speedTimer->start();
    
    // Queue processing timer
    connect(m_queueTimer, &QTimer::timeout, this, &DownloadManager::processQueue);
    m_queueTimer->setInterval(500);  // 500ms
    m_queueTimer->start();
}

DownloadManager::~DownloadManager() {
    // Stop timers
    m_speedTimer->stop();
    m_queueTimer->stop();
    
    // Clear tasks
    std::lock_guard lock(m_tasksMutex);
    m_tasks.clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Download Management
// ═══════════════════════════════════════════════════════════════════════════════

TaskId DownloadManager::addDownload(const QUrl& url, const QString& destPath, bool startImmediately) {
    if (!url.isValid()) {
        qWarning() << "DownloadManager: Invalid URL:" << url.toString();
        return TaskId{};
    }
    
    // Check for duplicate
    auto existing = findByUrl(url);
    if (existing.has_value()) {
        qWarning() << "DownloadManager: URL already exists:" << url.toString();
        return existing.value();
    }
    
    // Create task
    QString dest = destPath.isEmpty() ? m_defaultDir : destPath;
    auto task = std::make_unique<DownloadTask>(url, dest, this);
    TaskId id = task->id();
    
    // Connect task signals
    connectTask(task.get());
    
    // Store task
    {
        std::lock_guard lock(m_tasksMutex);
        m_tasks[id] = std::move(task);
    }
    
    qDebug() << "DownloadManager: Added download" << id.toString() << "URL:" << url.toString();
    
    updateCounts();
    emit downloadAdded(id);
    emit totalCountChanged();
    emit queueCountChanged();
    
    // Save to persistence
    if (m_persistence) {
        m_persistence->saveTask(m_tasks[id].get());
    }
    
    // Start if requested and we have capacity
    if (startImmediately && canStartMore()) {
        m_tasks[id]->start();
    }
    
    return id;
}

QString DownloadManager::addDownloadUrl(const QString& url, const QString& destPath, bool startImmediately) {
    TaskId id = addDownload(QUrl(url), destPath, startImmediately);
    return id.isNull() ? QString{} : id.toString(QUuid::WithoutBraces);
}

std::vector<TaskId> DownloadManager::addDownloads(const QList<QUrl>& urls, const QString& destDir) {
    std::vector<TaskId> ids;
    ids.reserve(urls.size());
    
    QString dest = destDir.isEmpty() ? m_defaultDir : destDir;
    
    for (const QUrl& url : urls) {
        TaskId id = addDownload(url, dest, false);
        if (!id.isNull()) {
            ids.push_back(id);
        }
    }
    
    // Start queued downloads
    processQueue();
    
    return ids;
}

void DownloadManager::removeDownload(const QString& id, bool deleteFile) {
    removeDownload(QUuid::fromString(id), deleteFile);
}

void DownloadManager::removeDownload(const TaskId& id, bool deleteFile) {
    std::unique_ptr<DownloadTask> task;
    
    {
        std::lock_guard lock(m_tasksMutex);
        auto it = m_tasks.find(id);
        if (it == m_tasks.end()) {
            return;
        }
        task = std::move(it->second);
        m_tasks.erase(it);
    }
    
    // Cancel if active
    if (task->isActive()) {
        task->cancel();
    }
    
    // Delete file if requested
    if (deleteFile && !task->filePath().isEmpty()) {
        QFile::remove(task->filePath());
    }
    
    // Remove from persistence
    if (m_persistence) {
        m_persistence->deleteTask(id);
    }
    
    disconnectTask(task.get());
    
    qDebug() << "DownloadManager: Removed download" << id.toString();
    
    updateCounts();
    emit downloadRemoved(id);
    emit totalCountChanged();
}

void DownloadManager::removeAllDownloads(bool deleteFiles) {
    std::vector<TaskId> ids;
    
    {
        std::lock_guard lock(m_tasksMutex);
        for (const auto& [id, task] : m_tasks) {
            ids.push_back(id);
        }
    }
    
    for (const TaskId& id : ids) {
        removeDownload(id, deleteFiles);
    }
}

void DownloadManager::clearCompleted() {
    std::vector<TaskId> completedIds;
    
    {
        std::lock_guard lock(m_tasksMutex);
        for (const auto& [id, task] : m_tasks) {
            if (task->state() == DownloadState::Completed) {
                completedIds.push_back(id);
            }
        }
    }
    
    for (const TaskId& id : completedIds) {
        removeDownload(id, false);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Task Access
// ═══════════════════════════════════════════════════════════════════════════════

DownloadTask* DownloadManager::task(const TaskId& id) const {
    std::lock_guard lock(m_tasksMutex);
    auto it = m_tasks.find(id);
    return it != m_tasks.end() ? it->second.get() : nullptr;
}

DownloadTask* DownloadManager::taskById(const QString& id) const {
    return task(QUuid::fromString(id));
}

std::vector<DownloadTask*> DownloadManager::allTasks() const {
    std::lock_guard lock(m_tasksMutex);
    
    std::vector<DownloadTask*> result;
    result.reserve(m_tasks.size());
    
    for (const auto& [id, task] : m_tasks) {
        result.push_back(task.get());
    }
    
    return result;
}

std::vector<DownloadTask*> DownloadManager::tasksInState(DownloadState state) const {
    std::lock_guard lock(m_tasksMutex);
    
    std::vector<DownloadTask*> result;
    
    for (const auto& [id, task] : m_tasks) {
        if (task->state() == state) {
            result.push_back(task.get());
        }
    }
    
    return result;
}

std::optional<TaskId> DownloadManager::findByUrl(const QUrl& url) const {
    std::lock_guard lock(m_tasksMutex);
    
    for (const auto& [id, task] : m_tasks) {
        if (task->urlObject() == url) {
            return id;
        }
    }
    
    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Bulk Actions
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadManager::startDownload(const QString& id) {
    startDownload(QUuid::fromString(id));
}

void DownloadManager::startDownload(const TaskId& id) {
    DownloadTask* t = task(id);
    if (t && canStartMore()) {
        t->start();
        emit downloadStarted(id);
    }
}

void DownloadManager::pauseDownload(const QString& id) {
    pauseDownload(QUuid::fromString(id));
}

void DownloadManager::pauseDownload(const TaskId& id) {
    DownloadTask* t = task(id);
    if (t) {
        t->pause();
        emit downloadPaused(id);
    }
}

void DownloadManager::resumeDownload(const QString& id) {
    resumeDownload(QUuid::fromString(id));
}

void DownloadManager::resumeDownload(const TaskId& id) {
    DownloadTask* t = task(id);
    if (t && canStartMore()) {
        t->resume();
        emit downloadResumed(id);
    }
}

void DownloadManager::cancelDownload(const QString& id) {
    cancelDownload(QUuid::fromString(id));
}

void DownloadManager::cancelDownload(const TaskId& id) {
    DownloadTask* t = task(id);
    if (t) {
        t->cancel();
    }
}

void DownloadManager::retryDownload(const QString& id) {
    retryDownload(QUuid::fromString(id));
}

void DownloadManager::retryDownload(const TaskId& id) {
    DownloadTask* t = task(id);
    if (t && canStartMore()) {
        t->retry();
    }
}

void DownloadManager::pauseAll() {
    auto tasks = tasksInState(DownloadState::Downloading);
    for (DownloadTask* t : tasks) {
        t->pause();
    }
}

void DownloadManager::resumeAll() {
    auto tasks = tasksInState(DownloadState::Paused);
    for (DownloadTask* t : tasks) {
        if (canStartMore()) {
            t->resume();
        }
    }
}

void DownloadManager::startAll() {
    auto tasks = tasksInState(DownloadState::Queued);
    for (DownloadTask* t : tasks) {
        if (canStartMore()) {
            t->start();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statistics
// ═══════════════════════════════════════════════════════════════════════════════

int DownloadManager::activeDownloadCount() const {
    return m_activeCount.load(std::memory_order_relaxed);
}

int DownloadManager::queuedDownloadCount() const {
    return m_queuedCount.load(std::memory_order_relaxed);
}

int DownloadManager::completedDownloadCount() const {
    return m_completedCount.load(std::memory_order_relaxed);
}

int DownloadManager::totalDownloadCount() const {
    std::lock_guard lock(m_tasksMutex);
    return static_cast<int>(m_tasks.size());
}

SpeedBps DownloadManager::globalSpeed() const {
    return m_globalSpeed.load(std::memory_order_relaxed);
}

ByteCount DownloadManager::totalBytesDownloaded() const {
    return m_totalBytesEver.load(std::memory_order_relaxed);
}

ByteCount DownloadManager::sessionBytesDownloaded() const {
    return m_sessionBytes.load(std::memory_order_relaxed);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Settings
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadManager::setMaxConcurrentDownloads(int count) {
    count = std::clamp(count, 1, 16);
    if (m_maxConcurrent != count) {
        m_maxConcurrent = count;
        emit settingsChanged();
        processQueue();
    }
}

void DownloadManager::setDefaultDownloadDirectory(const QString& path) {
    if (m_defaultDir != path) {
        m_defaultDir = path;
        QDir().mkpath(m_defaultDir);
        emit settingsChanged();
    }
}

void DownloadManager::setMaxSegmentsPerDownload(int count) {
    count = std::clamp(count, 1, static_cast<int>(Constants::MAX_SEGMENTS));
    if (m_maxSegments != count) {
        m_maxSegments = count;
        emit settingsChanged();
    }
}

void DownloadManager::setSpeedLimit(SpeedBps limit) {
    if (m_speedLimit != limit) {
        m_speedLimit = limit;
        emit settingsChanged();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Persistence
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadManager::saveState() {
    if (!m_persistence) return;
    
    qDebug() << "DownloadManager: Saving state...";
    
    std::lock_guard lock(m_tasksMutex);
    for (const auto& [id, task] : m_tasks) {
        m_persistence->saveTask(task.get());
    }
    
    m_persistence->checkpoint();
}

void DownloadManager::loadState() {
    if (!m_persistence) return;
    
    qDebug() << "DownloadManager: Loading state...";
    
    // Load tasks from database
    auto savedTasks = m_persistence->loadAllTasks();
    
    for (auto& taskData : savedTasks) {
        auto task = std::make_unique<DownloadTask>(
            taskData.id,
            QUrl(taskData.url),
            taskData.filePath,
            this
        );
        
        // Restore task state
        // (In a full implementation, this would restore all task properties)
        
        connectTask(task.get());
        
        std::lock_guard lock(m_tasksMutex);
        m_tasks[taskData.id] = std::move(task);
    }
    
    updateCounts();
    
    qDebug() << "DownloadManager: Loaded" << m_tasks.size() << "tasks";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Slots
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadManager::onTaskStateChanged(DownloadState newState) {
    updateCounts();
    
    auto* task = qobject_cast<DownloadTask*>(sender());
    if (!task) return;
    
    TaskId id = task->id();
    
    switch (newState) {
        case DownloadState::Downloading:
            emit activeCountChanged();
            break;
        case DownloadState::Paused:
            emit activeCountChanged();
            processQueue();
            break;
        case DownloadState::Completed:
            emit activeCountChanged();
            emit completedCountChanged();
            processQueue();
            break;
        case DownloadState::Failed:
            emit activeCountChanged();
            processQueue();
            break;
        default:
            break;
    }
}

void DownloadManager::onTaskCompleted() {
    auto* task = qobject_cast<DownloadTask*>(sender());
    if (task) {
        emit downloadCompleted(task->id());
    }
}

void DownloadManager::onTaskFailed(const DownloadError& error) {
    auto* task = qobject_cast<DownloadTask*>(sender());
    if (task) {
        emit downloadFailed(task->id(), error.message);
    }
}

void DownloadManager::onTaskNeedsPersistence() {
    auto* task = qobject_cast<DownloadTask*>(sender());
    if (task && m_persistence) {
        m_persistence->saveTask(task);
    }
}

void DownloadManager::onSpeedUpdateTimer() {
    SpeedBps totalSpeed = 0.0;
    ByteCount sessionBytes = 0;
    
    {
        std::lock_guard lock(m_tasksMutex);
        for (const auto& [id, task] : m_tasks) {
            if (task->isActive()) {
                totalSpeed += task->speed();
            }
            sessionBytes += task->downloadedSize();
        }
    }
    
    SpeedBps oldSpeed = m_globalSpeed.exchange(totalSpeed, std::memory_order_relaxed);
    m_sessionBytes.store(sessionBytes, std::memory_order_relaxed);
    
    if (oldSpeed != totalSpeed) {
        emit globalSpeedChanged();
    }
}

void DownloadManager::processQueue() {
    if (!canStartMore()) {
        return;
    }
    
    auto queued = tasksInState(DownloadState::Queued);
    
    // Sort by priority (higher priority first)
    std::sort(queued.begin(), queued.end(), [](DownloadTask* a, DownloadTask* b) {
        return a->priority() > b->priority();
    });
    
    for (DownloadTask* task : queued) {
        if (!canStartMore()) break;
        task->start();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadManager::connectTask(DownloadTask* task) {
    connect(task, &DownloadTask::stateChanged,
            this, &DownloadManager::onTaskStateChanged);
    connect(task, &DownloadTask::completed,
            this, &DownloadManager::onTaskCompleted);
    connect(task, &DownloadTask::failed,
            this, &DownloadManager::onTaskFailed);
    connect(task, &DownloadTask::needsPersistence,
            this, &DownloadManager::onTaskNeedsPersistence);
}

void DownloadManager::disconnectTask(DownloadTask* task) {
    disconnect(task, nullptr, this, nullptr);
}

void DownloadManager::updateCounts() {
    int active = 0, queued = 0, completed = 0;
    
    {
        std::lock_guard lock(m_tasksMutex);
        for (const auto& [id, task] : m_tasks) {
            switch (task->state()) {
                case DownloadState::Downloading:
                case DownloadState::Probing:
                case DownloadState::Merging:
                case DownloadState::Verifying:
                    ++active;
                    break;
                case DownloadState::Queued:
                    ++queued;
                    break;
                case DownloadState::Completed:
                    ++completed;
                    break;
                default:
                    break;
            }
        }
    }
    
    m_activeCount.store(active, std::memory_order_relaxed);
    m_queuedCount.store(queued, std::memory_order_relaxed);
    m_completedCount.store(completed, std::memory_order_relaxed);
}

bool DownloadManager::canStartMore() const {
    return activeDownloadCount() < m_maxConcurrent;
}

void DownloadManager::startNextQueued() {
    if (!canStartMore()) return;
    
    auto queued = tasksInState(DownloadState::Queued);
    if (!queued.empty()) {
        queued.front()->start();
    }
}

} // namespace OpenIDM
