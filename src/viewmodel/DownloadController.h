/**
 * @file DownloadController.h
 * @brief QML-invokable controller for download operations
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_DOWNLOADCONTROLLER_H
#define OPENIDM_DOWNLOADCONTROLLER_H

#include <QObject>
#include <QUrl>
#include <QQmlEngine>

#include "engine/DownloadTypes.h"

namespace OpenIDM {

class DownloadManager;
class DownloadListModel;

/**
 * @brief Controller for download operations from QML
 */
class DownloadController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(DownloadListModel* model READ model CONSTANT)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY queuedCountChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(double totalSpeed READ totalSpeed NOTIFY totalSpeedChanged)
    Q_PROPERTY(QString formattedTotalSpeed READ formattedTotalSpeed NOTIFY totalSpeedChanged)
    Q_PROPERTY(int maxConcurrent READ maxConcurrent WRITE setMaxConcurrent NOTIFY maxConcurrentChanged)
    Q_PROPERTY(QString defaultSavePath READ defaultSavePath WRITE setDefaultSavePath NOTIFY defaultSavePathChanged)

public:
    explicit DownloadController(QObject* parent = nullptr);
    ~DownloadController() override;

    static DownloadController* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    // ═══════════════════════════════════════════════════════════════════════════
    // Properties
    // ═══════════════════════════════════════════════════════════════════════════

    [[nodiscard]] DownloadListModel* model() const { return m_model; }
    [[nodiscard]] int activeCount() const;
    [[nodiscard]] int queuedCount() const;
    [[nodiscard]] int totalCount() const;
    [[nodiscard]] double totalSpeed() const;
    [[nodiscard]] QString formattedTotalSpeed() const;
    [[nodiscard]] int maxConcurrent() const;
    void setMaxConcurrent(int max);
    [[nodiscard]] QString defaultSavePath() const;
    void setDefaultSavePath(const QString& path);

    // ═══════════════════════════════════════════════════════════════════════════
    // QML-Invokable Methods
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Add a new download
     */
    Q_INVOKABLE QString addDownload(const QString& url,
                                     const QString& savePath = QString(),
                                     const QString& fileName = QString());

    /**
     * @brief Start a download
     */
    Q_INVOKABLE void startDownload(const QString& id);

    /**
     * @brief Pause a download
     */
    Q_INVOKABLE void pauseDownload(const QString& id);

    /**
     * @brief Resume a download
     */
    Q_INVOKABLE void resumeDownload(const QString& id);

    /**
     * @brief Cancel a download
     */
    Q_INVOKABLE void cancelDownload(const QString& id, bool deleteFiles = true);

    /**
     * @brief Retry a failed download
     */
    Q_INVOKABLE void retryDownload(const QString& id);

    /**
     * @brief Remove a download
     */
    Q_INVOKABLE void removeDownload(const QString& id, bool deleteFiles = false);

    /**
     * @brief Start all queued downloads
     */
    Q_INVOKABLE void startAll();

    /**
     * @brief Pause all active downloads
     */
    Q_INVOKABLE void pauseAll();

    /**
     * @brief Resume all paused downloads
     */
    Q_INVOKABLE void resumeAll();

    /**
     * @brief Clear completed downloads
     */
    Q_INVOKABLE void clearCompleted();

    /**
     * @brief Open file in system file manager
     */
    Q_INVOKABLE void openFile(const QString& id);

    /**
     * @brief Open containing folder
     */
    Q_INVOKABLE void openFolder(const QString& id);

    /**
     * @brief Copy URL to clipboard
     */
    Q_INVOKABLE void copyUrl(const QString& id);

    /**
     * @brief Validate URL
     */
    Q_INVOKABLE bool isValidUrl(const QString& url) const;

    /**
     * @brief Get clipboard text
     */
    Q_INVOKABLE QString getClipboardUrl() const;

signals:
    void activeCountChanged();
    void queuedCountChanged();
    void totalCountChanged();
    void totalSpeedChanged();
    void maxConcurrentChanged();
    void defaultSavePathChanged();

    void downloadAdded(const QString& id);
    void downloadCompleted(const QString& id, const QString& fileName);
    void downloadError(const QString& id, const QString& message);

private slots:
    void onDownloadCompleted(const QString& id);
    void onDownloadError(const QString& id, const QString& message);

private:
    DownloadManager* m_manager = nullptr;
    DownloadListModel* m_model = nullptr;

    static DownloadController* s_instance;
};

} // namespace OpenIDM

#endif // OPENIDM_DOWNLOADCONTROLLER_H
