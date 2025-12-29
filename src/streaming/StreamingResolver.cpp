/**
 * @file StreamingResolver.cpp
 * @brief Implementation of streaming URL resolver
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#include "StreamingResolver.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QFileInfo>
#include <QRegularExpression>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════════

StreamingResolver::StreamingResolver(QObject* parent)
    : QObject(parent)
{
    // Try to find yt-dlp
    m_ytdlpPath = findYtDlp();

    if (!m_ytdlpPath.isEmpty()) {
        qDebug() << "StreamingResolver: Found yt-dlp at" << m_ytdlpPath;
    } else {
        qDebug() << "StreamingResolver: yt-dlp not found";
    }
}

StreamingResolver::~StreamingResolver()
{
    cancel();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void StreamingResolver::setYtDlpPath(const QString& path)
{
    m_ytdlpPath = path;
}

void StreamingResolver::setPreferredFormat(const QString& format)
{
    m_preferredFormat = format;
}

void StreamingResolver::setPreferredQuality(const QString& quality)
{
    m_preferredQuality = quality;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Status
// ═══════════════════════════════════════════════════════════════════════════════

bool StreamingResolver::isAvailable() const
{
    if (m_ytdlpPath.isEmpty()) {
        return false;
    }

    QFileInfo info(m_ytdlpPath);
    return info.exists() && info.isExecutable();
}

QString StreamingResolver::version() const
{
    if (!isAvailable()) {
        return QString();
    }

    QProcess process;
    process.start(m_ytdlpPath, {"--version"});

    if (process.waitForFinished(5000)) {
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }

    return QString();
}

bool StreamingResolver::isSupportedUrl(const QUrl& url) const
{
    QString host = url.host().toLower();

    // Common streaming services
    static const QStringList supportedHosts = {
        "youtube.com", "www.youtube.com", "youtu.be", "m.youtube.com",
        "vimeo.com", "www.vimeo.com",
        "twitter.com", "www.twitter.com", "x.com", "www.x.com",
        "instagram.com", "www.instagram.com",
        "tiktok.com", "www.tiktok.com",
        "facebook.com", "www.facebook.com", "fb.watch",
        "twitch.tv", "www.twitch.tv",
        "dailymotion.com", "www.dailymotion.com",
        "soundcloud.com", "www.soundcloud.com",
        "bandcamp.com",
        "reddit.com", "www.reddit.com", "v.redd.it"
    };

    for (const auto& supported : supportedHosts) {
        if (host.endsWith(supported)) {
            return true;
        }
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Resolution
// ═══════════════════════════════════════════════════════════════════════════════

void StreamingResolver::resolve(const QUrl& url)
{
    if (!isAvailable()) {
        emit error("yt-dlp is not available");
        return;
    }

    // Cancel any existing resolution
    cancel();

    m_currentUrl = url;
    m_outputBuffer.clear();

    // Create process
    m_process = new QProcess(this);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &StreamingResolver::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &StreamingResolver::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &StreamingResolver::onProcessReadyRead);

    // Build arguments
    QStringList args;
    args << "--no-playlist";      // Single video only
    args << "--no-warnings";      // Suppress warnings
    args << "-j";                 // JSON output

    // Format selection
    if (!m_preferredFormat.isEmpty()) {
        args << "-f" << m_preferredFormat;
    }

    args << url.toString();

    qDebug() << "StreamingResolver: Starting yt-dlp with args:" << args;

    emit progress("Resolving URL...");

    m_process->start(m_ytdlpPath, args);
}

void StreamingResolver::cancel()
{
    if (m_process) {
        m_process->disconnect();

        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(3000);
        }

        m_process->deleteLater();
        m_process = nullptr;
    }

    m_outputBuffer.clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Slots
// ═══════════════════════════════════════════════════════════════════════════════

void StreamingResolver::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status != QProcess::NormalExit || exitCode != 0) {
        QString errorOutput = QString::fromUtf8(m_process->readAllStandardError());
        qWarning() << "StreamingResolver: yt-dlp failed:" << errorOutput;
        emit error(errorOutput.isEmpty() ? "Failed to resolve URL" : errorOutput);
    } else {
        parseOutput(m_outputBuffer);
    }

    m_process->deleteLater();
    m_process = nullptr;
}

void StreamingResolver::onProcessError(QProcess::ProcessError processError)
{
    QString errorMsg;

    switch (processError) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start yt-dlp";
            break;
        case QProcess::Crashed:
            errorMsg = "yt-dlp crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "yt-dlp timed out";
            break;
        default:
            errorMsg = "yt-dlp error";
            break;
    }

    qWarning() << "StreamingResolver:" << errorMsg;
    emit error(errorMsg);

    m_process->deleteLater();
    m_process = nullptr;
}

void StreamingResolver::onProcessReadyRead()
{
    m_outputBuffer.append(m_process->readAllStandardOutput());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Helpers
// ═══════════════════════════════════════════════════════════════════════════════

QString StreamingResolver::findYtDlp() const
{
    // Check common locations
    QStringList searchPaths;

#ifdef Q_OS_WIN
    searchPaths << "yt-dlp.exe";
    searchPaths << "yt-dlp";

    // Check in PATH
    QString pathEnv = qEnvironmentVariable("PATH");
    for (const auto& dir : pathEnv.split(';')) {
        searchPaths << dir + "/yt-dlp.exe";
        searchPaths << dir + "/yt-dlp";
    }

    // Common Windows locations
    searchPaths << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/yt-dlp.exe";
#else
    searchPaths << "yt-dlp";

    // Check in PATH
    QString pathEnv = qEnvironmentVariable("PATH");
    for (const auto& dir : pathEnv.split(':')) {
        searchPaths << dir + "/yt-dlp";
    }

    // Common Unix locations
    searchPaths << "/usr/local/bin/yt-dlp";
    searchPaths << "/usr/bin/yt-dlp";
    searchPaths << QDir::homePath() + "/.local/bin/yt-dlp";
#endif

    for (const auto& path : searchPaths) {
        QFileInfo info(path);
        if (info.exists() && info.isExecutable()) {
            return info.absoluteFilePath();
        }
    }

    // Try to find via which/where
    QProcess which;
#ifdef Q_OS_WIN
    which.start("where", {"yt-dlp"});
#else
    which.start("which", {"yt-dlp"});
#endif

    if (which.waitForFinished(3000) && which.exitCode() == 0) {
        QString result = QString::fromUtf8(which.readAllStandardOutput()).trimmed();
        if (!result.isEmpty()) {
            return result.split('\n').first();
        }
    }

    return QString();
}

void StreamingResolver::parseOutput(const QByteArray& output)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(output, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "StreamingResolver: JSON parse error:" << parseError.errorString();
        emit error("Failed to parse yt-dlp output");
        return;
    }

    if (!doc.isObject()) {
        emit error("Invalid yt-dlp output format");
        return;
    }

    QJsonObject obj = doc.object();

    // Extract information
    StreamInfo info;
    info.url = QUrl(obj["url"].toString());
    info.title = obj["title"].toString();
    info.extension = obj["ext"].toString();
    info.format = obj["format"].toString();
    info.fileSize = obj["filesize"].toVariant().toLongLong();
    info.duration = obj["duration"].toInt();
    info.thumbnail = obj["thumbnail"].toString();
    info.description = obj["description"].toString();
    info.uploader = obj["uploader"].toString();

    if (info.url.isEmpty()) {
        // Try to get URL from requested_formats
        QJsonArray formats = obj["requested_formats"].toArray();
        if (!formats.isEmpty()) {
            info.url = QUrl(formats.first().toObject()["url"].toString());
        }
    }

    if (info.url.isEmpty()) {
        emit error("Could not extract download URL");
        return;
    }

    qDebug() << "StreamingResolver: Resolved URL:" << info.url.toString();
    qDebug() << "StreamingResolver: Title:" << info.title;
    qDebug() << "StreamingResolver: Format:" << info.format;

    emit resolved(info.url);
    emit resolvedWithInfo(info);
}

} // namespace OpenIDM
