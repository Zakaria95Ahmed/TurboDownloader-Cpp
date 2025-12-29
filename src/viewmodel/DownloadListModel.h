/**
 * @file DownloadListModel.h
 * @brief Qt model for exposing downloads to QML
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_DOWNLOADLISTMODEL_H
#define OPENIDM_DOWNLOADLISTMODEL_H

#include <QAbstractListModel>
#include <QTimer>

#include "engine/DownloadTypes.h"

namespace OpenIDM {

class DownloadManager;

/**
 * @brief Qt model exposing downloads to QML ListView
 */
class DownloadListModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        FileNameRole,
        UrlRole,
        SavePathRole,
        TotalSizeRole,
        DownloadedBytesRole,
        ProgressRole,
        SpeedRole,
        FormattedSpeedRole,
        EtaRole,
        FormattedEtaRole,
        StateRole,
        StateTextRole,
        ErrorMessageRole,
        PriorityRole,
        ActiveSegmentsRole,
        MaxSegmentsRole,
        CreatedAtRole,
        CompletedAtRole,
        ContentTypeRole,
        FormattedSizeRole
    };
    Q_ENUM(Roles)

    explicit DownloadListModel(QObject* parent = nullptr);
    ~DownloadListModel() override;

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief Get download ID at index
     */
    Q_INVOKABLE QString idAt(int index) const;

    /**
     * @brief Find index by download ID
     */
    Q_INVOKABLE int indexOf(const QString& id) const;

signals:
    void countChanged();

private slots:
    void onDownloadAdded(const QString& id);
    void onDownloadRemoved(const QString& id);
    void onDownloadStateChanged(const QString& id, DownloadState newState);
    void onDownloadProgressUpdated(const QString& id, qint64 downloaded,
                                    qint64 total, double percent);
    void onRefreshTimer();

private:
    void refreshData();
    QString formatSize(qint64 bytes) const;
    QString formatSpeed(double bytesPerSecond) const;
    QString formatEta(qint64 seconds) const;
    QString stateToText(DownloadState state) const;

    DownloadManager* m_manager = nullptr;
    std::vector<DownloadInfo> m_downloads;
    QTimer* m_refreshTimer = nullptr;
};

} // namespace OpenIDM

#endif // OPENIDM_DOWNLOADLISTMODEL_H
