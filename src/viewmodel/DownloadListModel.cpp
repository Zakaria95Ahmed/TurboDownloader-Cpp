/**
 * @file DownloadListModel.cpp
 * @brief Implementation of DownloadListModel
 */

#include "openidm/viewmodel/DownloadListModel.h"
#include "openidm/engine/DownloadManager.h"
#include "openidm/engine/DownloadTask.h"

namespace OpenIDM {

DownloadListModel::DownloadListModel(DownloadManager* manager, QObject* parent)
    : QAbstractListModel(parent)
    , m_manager(manager)
    , m_updateTimer(new QTimer(this))
{
    // Setup role names for QML
    m_roleNames[IdRole] = "id";
    m_roleNames[UrlRole] = "url";
    m_roleNames[FileNameRole] = "fileName";
    m_roleNames[FilePathRole] = "filePath";
    m_roleNames[TotalSizeRole] = "totalSize";
    m_roleNames[DownloadedSizeRole] = "downloadedSize";
    m_roleNames[ProgressRole] = "progress";
    m_roleNames[SpeedRole] = "speed";
    m_roleNames[SpeedFormattedRole] = "speedFormatted";
    m_roleNames[StateRole] = "state";
    m_roleNames[StateStringRole] = "stateString";
    m_roleNames[RemainingTimeRole] = "remainingTime";
    m_roleNames[ErrorMessageRole] = "errorMessage";
    m_roleNames[ActiveSegmentsRole] = "activeSegments";
    m_roleNames[TotalSegmentsRole] = "totalSegments";
    m_roleNames[ContentTypeRole] = "contentType";
    m_roleNames[PriorityRole] = "priority";
    
    // Connect to manager signals
    connect(m_manager, &DownloadManager::downloadAdded,
            this, &DownloadListModel::onDownloadAdded);
    connect(m_manager, &DownloadManager::downloadRemoved,
            this, &DownloadListModel::onDownloadRemoved);
    
    // Setup update timer for progress updates
    connect(m_updateTimer, &QTimer::timeout,
            this, &DownloadListModel::onProgressTimer);
    m_updateTimer->setInterval(100);  // 100ms updates
    m_updateTimer->start();
    
    // Initial load
    refreshTaskList();
}

DownloadListModel::~DownloadListModel() {
    m_updateTimer->stop();
}

int DownloadListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_tasks.size();
}

QVariant DownloadListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_tasks.size()) {
        return QVariant();
    }
    
    DownloadTask* task = m_tasks.at(index.row());
    if (!task) {
        return QVariant();
    }
    
    switch (role) {
        case IdRole:
            return task->idString();
        case UrlRole:
            return task->url();
        case FileNameRole:
            return task->fileName();
        case FilePathRole:
            return task->filePath();
        case TotalSizeRole:
            return task->totalSize();
        case DownloadedSizeRole:
            return task->downloadedSize();
        case ProgressRole:
            return task->progress();
        case SpeedRole:
            return task->speed();
        case SpeedFormattedRole:
            return task->speedFormatted();
        case StateRole:
            return task->stateInt();
        case StateStringRole:
            return task->stateString();
        case RemainingTimeRole:
            return task->remainingTimeFormatted();
        case ErrorMessageRole:
            return task->errorMessage();
        case ActiveSegmentsRole:
            return task->activeSegments();
        case TotalSegmentsRole:
            return task->totalSegments();
        case ContentTypeRole:
            return task->contentType();
        case PriorityRole:
            return static_cast<int>(task->priority());
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> DownloadListModel::roleNames() const {
    return m_roleNames;
}

int DownloadListModel::indexOf(const QString& taskId) const {
    QUuid id = QUuid::fromString(taskId);
    return findTaskIndex(id);
}

QString DownloadListModel::taskIdAt(int index) const {
    if (index >= 0 && index < m_tasks.size()) {
        return m_tasks.at(index)->idString();
    }
    return QString();
}

void DownloadListModel::onDownloadAdded(const QUuid& id) {
    DownloadTask* task = m_manager->task(id);
    if (!task) return;
    
    beginInsertRows(QModelIndex(), m_tasks.size(), m_tasks.size());
    m_tasks.append(task);
    endInsertRows();
    
    emit countChanged();
}

void DownloadListModel::onDownloadRemoved(const QUuid& id) {
    int index = findTaskIndex(id);
    if (index < 0) return;
    
    beginRemoveRows(QModelIndex(), index, index);
    m_tasks.removeAt(index);
    endRemoveRows();
    
    emit countChanged();
}

void DownloadListModel::onProgressTimer() {
    // Emit dataChanged for all active downloads
    for (int i = 0; i < m_tasks.size(); ++i) {
        DownloadTask* task = m_tasks.at(i);
        if (task->isActive()) {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {
                ProgressRole,
                SpeedRole,
                SpeedFormattedRole,
                DownloadedSizeRole,
                RemainingTimeRole,
                ActiveSegmentsRole,
                StateRole,
                StateStringRole
            });
        }
    }
}

void DownloadListModel::refreshTaskList() {
    beginResetModel();
    m_tasks.clear();
    
    auto allTasks = m_manager->allTasks();
    for (DownloadTask* task : allTasks) {
        m_tasks.append(task);
    }
    
    endResetModel();
    emit countChanged();
}

int DownloadListModel::findTaskIndex(const QUuid& id) const {
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks.at(i)->id() == id) {
            return i;
        }
    }
    return -1;
}

} // namespace OpenIDM
