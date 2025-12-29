/**
 * @file YtDlpIntegration.cpp
 * @brief Integration with yt-dlp for streaming site support
 * 
 * This module provides a thin wrapper around yt-dlp to support
 * downloading from YouTube, Vimeo, and 1000+ other streaming sites.
 * 
 * License Note: yt-dlp is GPL licensed. This integration uses it
 * as an external process, maintaining license compatibility.
 */

#include "openidm/integration/YtDlpIntegration.h"

#include <QProcess>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// YtDlpIntegration Implementation
// ═══════════════════════════════════════════════════════════════════════════════

YtDlpIntegration::YtDlpIntegration(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    // Connect process signals
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &YtDlpIntegration::onProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &YtDlpIntegration::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &YtDlpIntegration::onProcessFinished);
    
    // Find yt-dlp executable
    m_ytdlpPath = findYtDlp();
}

YtDlpIntegration::~YtDlpIntegration() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool YtDlpIntegration::isAvailable() const {
    return !m_ytdlpPath.isEmpty() && QFile::exists(m_ytdlpPath);
}

QString YtDlpIntegration::version() const {
    if (!isAvailable()) {
        return QString();
    }
    
    QProcess process;
    process.start(m_ytdlpPath, {QStringLiteral("--version")});
    process.waitForFinished(5000);
    
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

bool YtDlpIntegration::isSupportedUrl(const QUrl& url) const {
    QString host = url.host().toLower();
    
    // Common supported sites (not exhaustive)
    static const QStringList supportedHosts = {
        QStringLiteral("youtube.com"),
        QStringLiteral("youtu.be"),
        QStringLiteral("vimeo.com"),
        QStringLiteral("dailymotion.com"),
        QStringLiteral("twitch.tv"),
        QStringLiteral("twitter.com"),
        QStringLiteral("x.com"),
        QStringLiteral("instagram.com"),
        QStringLiteral("facebook.com"),
        QStringLiteral("tiktok.com"),
        QStringLiteral("reddit.com"),
        QStringLiteral("soundcloud.com"),
        QStringLiteral("bandcamp.com"),
        QStringLiteral("bilibili.com"),
        QStringLiteral("nicovideo.jp"),
    };
    
    for (const QString& supported : supportedHosts) {
        if (host.endsWith(supported) || host == supported) {
            return true;
        }
    }
    
    // Also check for generic patterns
    if (url.toString().contains(QStringLiteral(".m3u8")) ||
        url.toString().contains(QStringLiteral("manifest")) ||
        url.toString().contains(QStringLiteral("playlist"))) {
        return true;
    }
    
    return false;
}

void YtDlpIntegration::extractInfo(const QUrl& url) {
    if (!isAvailable()) {
        emit error(QStringLiteral("yt-dlp is not installed or not found in PATH"));
        return;
    }
    
    if (m_process->state() != QProcess::NotRunning) {
        emit error(QStringLiteral("Another extraction is in progress"));
        return;
    }
    
    m_currentUrl = url;
    m_outputBuffer.clear();
    m_errorBuffer.clear();
    
    QStringList args = {
        QStringLiteral("--dump-json"),
        QStringLiteral("--no-playlist"),
        QStringLiteral("--no-download"),
        url.toString()
    };
    
    qDebug() << "YtDlpIntegration: Extracting info for" << url.toString();
    m_process->start(m_ytdlpPath, args);
}

void YtDlpIntegration::download(const QUrl& url, const QString& outputPath, 
                                 const QString& format) {
    if (!isAvailable()) {
        emit error(QStringLiteral("yt-dlp is not installed or not found in PATH"));
        return;
    }
    
    if (m_process->state() != QProcess::NotRunning) {
        emit error(QStringLiteral("Another download is in progress"));
        return;
    }
    
    m_currentUrl = url;
    m_outputBuffer.clear();
    m_errorBuffer.clear();
    
    QStringList args = {
        QStringLiteral("--newline"),  // Progress on new lines
        QStringLiteral("--progress"),
        QStringLiteral("-o"), outputPath
    };
    
    if (!format.isEmpty()) {
        args << QStringLiteral("-f") << format;
    }
    
    args << url.toString();
    
    qDebug() << "YtDlpIntegration: Starting download" << url.toString();
    m_process->start(m_ytdlpPath, args);
}

void YtDlpIntegration::cancel() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

void YtDlpIntegration::onProcessOutput() {
    QByteArray data = m_process->readAllStandardOutput();
    m_outputBuffer.append(data);
    
    // Parse progress updates
    QString output = QString::fromUtf8(data);
    
    // Look for progress pattern: [download] XX.X% of ~XXX MiB at XXX MiB/s
    static QRegularExpression progressRegex(
        QStringLiteral("\\[download\\]\\s+(\\d+\\.?\\d*)%.*?at\\s+([\\d.]+)\\s*(\\w+)/s")
    );
    
    auto match = progressRegex.match(output);
    if (match.hasMatch()) {
        double percent = match.captured(1).toDouble();
        double speed = match.captured(2).toDouble();
        QString unit = match.captured(3);
        
        // Convert speed to bytes/sec
        if (unit == QStringLiteral("KiB")) speed *= 1024;
        else if (unit == QStringLiteral("MiB")) speed *= 1024 * 1024;
        else if (unit == QStringLiteral("GiB")) speed *= 1024 * 1024 * 1024;
        
        emit progress(percent, speed);
    }
}

void YtDlpIntegration::onProcessError() {
    m_errorBuffer.append(m_process->readAllStandardError());
}

void YtDlpIntegration::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus == QProcess::CrashExit) {
        emit error(QStringLiteral("yt-dlp process crashed"));
        return;
    }
    
    if (exitCode != 0) {
        QString errorMsg = QString::fromUtf8(m_errorBuffer);
        if (errorMsg.isEmpty()) {
            errorMsg = QStringLiteral("yt-dlp exited with code %1").arg(exitCode);
        }
        emit error(errorMsg);
        return;
    }
    
    // Parse JSON output for info extraction
    QString output = QString::fromUtf8(m_outputBuffer);
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(m_outputBuffer, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        // Not JSON, might be download output
        emit finished(m_currentUrl.toString());
        return;
    }
    
    if (!doc.isObject()) {
        emit error(QStringLiteral("Invalid JSON response from yt-dlp"));
        return;
    }
    
    // Parse video info
    QJsonObject obj = doc.object();
    VideoInfo info;
    info.url = m_currentUrl.toString();
    info.title = obj.value(QStringLiteral("title")).toString();
    info.description = obj.value(QStringLiteral("description")).toString();
    info.duration = obj.value(QStringLiteral("duration")).toInt();
    info.thumbnail = obj.value(QStringLiteral("thumbnail")).toString();
    info.uploader = obj.value(QStringLiteral("uploader")).toString();
    info.uploadDate = obj.value(QStringLiteral("upload_date")).toString();
    
    // Parse formats
    QJsonArray formats = obj.value(QStringLiteral("formats")).toArray();
    for (const QJsonValue& formatVal : formats) {
        QJsonObject formatObj = formatVal.toObject();
        
        FormatInfo format;
        format.formatId = formatObj.value(QStringLiteral("format_id")).toString();
        format.ext = formatObj.value(QStringLiteral("ext")).toString();
        format.resolution = formatObj.value(QStringLiteral("resolution")).toString();
        format.filesize = formatObj.value(QStringLiteral("filesize")).toVariant().toLongLong();
        format.vcodec = formatObj.value(QStringLiteral("vcodec")).toString();
        format.acodec = formatObj.value(QStringLiteral("acodec")).toString();
        format.tbr = formatObj.value(QStringLiteral("tbr")).toDouble();
        format.note = formatObj.value(QStringLiteral("format_note")).toString();
        
        info.formats.append(format);
    }
    
    // Best format
    info.bestFormat = obj.value(QStringLiteral("format_id")).toString();
    info.directUrl = obj.value(QStringLiteral("url")).toString();
    
    emit infoExtracted(info);
}

QString YtDlpIntegration::findYtDlp() const {
    // Check common locations
    QStringList candidates = {
        QStringLiteral("yt-dlp"),
        QStringLiteral("yt-dlp.exe"),
    };
    
    // Check PATH first
    QString found = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!found.isEmpty()) {
        return found;
    }
    
    // Check common install locations
#ifdef Q_OS_WIN
    QStringList paths = {
        QDir::homePath() + QStringLiteral("/AppData/Local/Programs/yt-dlp"),
        QStringLiteral("C:/Program Files/yt-dlp"),
        QStringLiteral("C:/yt-dlp"),
    };
#else
    QStringList paths = {
        QStringLiteral("/usr/local/bin"),
        QStringLiteral("/usr/bin"),
        QDir::homePath() + QStringLiteral("/.local/bin"),
    };
#endif
    
    for (const QString& path : paths) {
        for (const QString& candidate : candidates) {
            QString fullPath = path + QDir::separator() + candidate;
            if (QFile::exists(fullPath)) {
                return fullPath;
            }
        }
    }
    
    return QString();
}

} // namespace OpenIDM
