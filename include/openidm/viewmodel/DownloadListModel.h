/**
 * @file DownloadListModel.h
 * @brief Qt List Model for download items (MVVM ViewModel)
 */

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QTimer>

namespace OpenIDM {

class DownloadManager;
class DownloadTask;

/**
 * @class DownloadListModel
 * @brief QAbstractListModel implementation for download list
 * 
 * Provides a Qt model interface for QML ListView binding.
 * Implements role-based data access for download properties.
 */
class DownloadListModel : public QAbstractListModel {
    Q_OBJECT
    
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    // Model roles
    enum DownloadRoles {
        IdRole = Qt::UserRole + 1,
        UrlRole,
        FileNameRole,
        FilePathRole,
        TotalSizeRole,
        DownloadedSizeRole,
        ProgressRole,
        SpeedRole,
        SpeedFormattedRole,
        StateRole,
        StateStringRole,
        RemainingTimeRole,
        ErrorMessageRole,
        ActiveSegmentsRole,
        TotalSegmentsRole,
        ContentTypeRole,
        PriorityRole
    };
    Q_ENUM(DownloadRoles)
    
    explicit DownloadListModel(DownloadManager* manager, QObject* parent = nullptr);
    ~DownloadListModel() override;
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // Model manipulation
    Q_INVOKABLE int indexOf(const QString& taskId) const;
    Q_INVOKABLE QString taskIdAt(int index) const;
    
signals:
    void countChanged();
    
private slots:
    void onDownloadAdded(const QUuid& id);
    void onDownloadRemoved(const QUuid& id);
    void onProgressTimer();
    
private:
    void refreshTaskList();
    int findTaskIndex(const QUuid& id) const;
    
    DownloadManager* m_manager;
    QList<DownloadTask*> m_tasks;
    QHash<int, QByteArray> m_roleNames;
    QTimer* m_updateTimer;
};

} // namespace OpenIDM
