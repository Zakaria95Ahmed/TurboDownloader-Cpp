/**
 * @file DownloadController.cpp
 * @brief Implementation of QML download controller
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#include "DownloadController.h"
#include "DownloadListModel.h"
#include "engine/DownloadManager.h"

#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>

namespace OpenIDM {

DownloadController* DownloadController::s_instance = nullptr;

DownloadController::DownloadController(QObject* parent)
    : QObject(parent)
{
    m_manager = DownloadManager::instance();
    m_model = new DownloadListModel(this);

    // Connect signals
    connect(m_manager, &DownloadManager::activeCountChanged,
            this, &DownloadController::activeCountChanged);
    connect(m_manager, &DownloadManager::queuedCountChanged,
            this, &DownloadController::queuedCountChanged);
    connect(m_manager, &DownloadManager::totalCountChanged,
            this, &DownloadController::totalCountChanged);
    connect(m_manager, &DownloadManager::totalSpeedUpdated,
            this, &DownloadController::totalSpeedChanged);
    connect(m_manager, &DownloadManager::maxConcurrentChanged,
            this, &DownloadController::maxConcurrentChanged);
    connect(m_manager, &DownloadManager::downloadCompleted,
            this, &DownloadController::onDownloadCompleted);
    connect(m_manager, &DownloadManager::downloadError,
            this, &DownloadController::onDownloadError);
    connect(m_manager, &DownloadManager::downloadAdded,
            this, &DownloadController::downloadAdded);

    s_instance = this;
}

DownloadController::~DownloadController()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

DownloadController* DownloadController::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!s_instance) {
        s_instance = new DownloadController();
    }

    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Properties
// ═══════════════════════════════════════════════════════════════════════════════

int DownloadController::activeCount() const
{
    return m_manager->activeCount();
}

int DownloadController::queuedCount() const
{
    return m_manager->queuedCount();
}

int DownloadController::totalCount() const
{
    return m_manager->totalCount();
}

double DownloadController::totalSpeed() const
{
    return m_manager->totalSpeed();
}

QString DownloadController::formattedTotalSpeed() const
{
    double speed = totalSpeed();

    if (speed < 1024) {
        return QString::number(speed, 'f', 0) + " B/s";
    }
    if (speed < 1024 * 1024) {
        return QString::number(speed / 1024.0, 'f', 1) + " KB/s";
    }
    if (speed < 1024 * 1024 * 1024) {
        return QString::number(speed / (1024.0 * 1024.0), 'f', 2) + " MB/s";
    }
    return QString::number(speed / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB/s";
}

int DownloadController::maxConcurrent() const
{
    return m_manager->maxConcurrent();
}

void DownloadController::setMaxConcurrent(int max)
{
    m_manager->setMaxConcurrent(max);
}

QString DownloadController::defaultSavePath() const
{
    return m_manager->defaultSavePath();
}

void DownloadController::setDefaultSavePath(const QString& path)
{
    m_manager->setDefaultSavePath(path);
    emit defaultSavePathChanged();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Download Operations
// ═══════════════════════════════════════════════════════════════════════════════

QString DownloadController::addDownload(const QString& url,
                                         const QString& savePath,
                                         const QString& fileName)
{
    if (!isValidUrl(url)) {
        qWarning() << "DownloadController: Invalid URL:" << url;
        return QString();
    }

    return m_manager->addDownload(QUrl(url), savePath, fileName);
}

void DownloadController::startDownload(const QString& id)
{
    m_manager->startDownload(id);
}

void DownloadController::pauseDownload(const QString& id)
{
    m_manager->pauseDownload(id);
}

void DownloadController::resumeDownload(const QString& id)
{
    m_manager->resumeDownload(id);
}

void DownloadController::cancelDownload(const QString& id, bool deleteFiles)
{
    m_manager->cancelDownload(id, deleteFiles);
}

void DownloadController::retryDownload(const QString& id)
{
    m_manager->retryDownload(id);
}

void DownloadController::removeDownload(const QString& id, bool deleteFiles)
{
    m_manager->removeDownload(id, deleteFiles);
}

void DownloadController::startAll()
{
    m_manager->startAll();
}

void DownloadController::pauseAll()
{
    m_manager->pauseAll();
}

void DownloadController::resumeAll()
{
    m_manager->resumeAll();
}

void DownloadController::clearCompleted()
{
    m_manager->clearCompleted();
}

void DownloadController::openFile(const QString& id)
{
    DownloadInfo info = m_manager->downloadInfo(id);
    if (info.id.isEmpty()) {
        return;
    }

    QString filePath = info.fullFilePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void DownloadController::openFolder(const QString& id)
{
    DownloadInfo info = m_manager->downloadInfo(id);
    if (info.id.isEmpty()) {
        return;
    }

    QString filePath = info.fullFilePath();
    QFileInfo fileInfo(filePath);

#ifdef Q_OS_WIN
    // On Windows, select the file in Explorer
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(filePath)});
#elif defined(Q_OS_MAC)
    // On macOS, reveal in Finder
    QProcess::startDetached("open", {"-R", filePath});
#else
    // On Linux, open the containing folder
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.path()));
#endif
}

void DownloadController::copyUrl(const QString& id)
{
    DownloadInfo info = m_manager->downloadInfo(id);
    if (info.id.isEmpty()) {
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(info.originalUrl.toString());
}

bool DownloadController::isValidUrl(const QString& url) const
{
    QUrl parsed(url);

    if (!parsed.isValid()) {
        return false;
    }

    QString scheme = parsed.scheme().toLower();
    if (scheme != "http" && scheme != "https" && scheme != "ftp" && scheme != "ftps") {
        return false;
    }

    if (parsed.host().isEmpty()) {
        return false;
    }

    return true;
}

QString DownloadController::getClipboardUrl() const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    QString text = clipboard->text().trimmed();

    if (isValidUrl(text)) {
        return text;
    }

    return QString();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Slots
// ═══════════════════════════════════════════════════════════════════════════════

void DownloadController::onDownloadCompleted(const QString& id)
{
    DownloadInfo info = m_manager->downloadInfo(id);
    emit downloadCompleted(id, info.fileName);
}

void DownloadController::onDownloadError(const QString& id, const QString& message)
{
    emit downloadError(id, message);
}

} // namespace OpenIDM
