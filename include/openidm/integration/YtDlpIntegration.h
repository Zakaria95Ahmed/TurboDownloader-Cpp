/**
 * @file YtDlpIntegration.h
 * @brief Integration with yt-dlp for streaming site support
 */

#pragma once

#include <QObject>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QRegularExpression>

namespace OpenIDM {

/**
 * @brief Information about a video format option
 */
struct FormatInfo {
    QString formatId;       ///< Format identifier
    QString ext;            ///< File extension
    QString resolution;     ///< Video resolution (e.g., "1920x1080")
    qint64 filesize;        ///< File size in bytes (-1 if unknown)
    QString vcodec;         ///< Video codec
    QString acodec;         ///< Audio codec
    double tbr;             ///< Total bitrate
    QString note;           ///< Format note (e.g., "1080p")
};

/**
 * @brief Extracted video information
 */
struct VideoInfo {
    QString url;            ///< Original URL
    QString title;          ///< Video title
    QString description;    ///< Video description
    int duration;           ///< Duration in seconds
    QString thumbnail;      ///< Thumbnail URL
    QString uploader;       ///< Uploader name
    QString uploadDate;     ///< Upload date (YYYYMMDD)
    QString bestFormat;     ///< Best format ID
    QString directUrl;      ///< Direct download URL
    QList<FormatInfo> formats;  ///< Available formats
};

/**
 * @class YtDlpIntegration
 * @brief Wrapper for yt-dlp external process
 * 
 * Provides functionality to:
 * - Check if yt-dlp is installed
 * - Extract video information and formats
 * - Download videos with progress reporting
 * - Support for playlists and streams
 */
class YtDlpIntegration : public QObject {
    Q_OBJECT
    
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit YtDlpIntegration(QObject* parent = nullptr);
    ~YtDlpIntegration() override;
    
    /**
     * @brief Check if yt-dlp is available
     */
    bool isAvailable() const;
    
    /**
     * @brief Get yt-dlp version string
     */
    QString version() const;
    
    /**
     * @brief Check if URL is supported by yt-dlp
     * @param url URL to check
     * @return True if URL matches known streaming sites
     */
    Q_INVOKABLE bool isSupportedUrl(const QUrl& url) const;
    
    /**
     * @brief Extract video information
     * @param url Video URL
     * 
     * Emits infoExtracted() on success or error() on failure.
     */
    Q_INVOKABLE void extractInfo(const QUrl& url);
    
    /**
     * @brief Download video
     * @param url Video URL
     * @param outputPath Output file path (with template placeholders)
     * @param format Format ID (empty for best)
     * 
     * Emits progress() during download, finished() or error() at end.
     */
    Q_INVOKABLE void download(const QUrl& url, 
                               const QString& outputPath,
                               const QString& format = QString());
    
    /**
     * @brief Cancel ongoing operation
     */
    Q_INVOKABLE void cancel();
    
signals:
    /**
     * @brief Video information extracted successfully
     * @param info Extracted video information
     */
    void infoExtracted(const VideoInfo& info);
    
    /**
     * @brief Download progress update
     * @param percent Progress percentage (0-100)
     * @param speed Download speed in bytes/sec
     */
    void progress(double percent, double speed);
    
    /**
     * @brief Operation completed successfully
     * @param outputPath Path to downloaded file
     */
    void finished(const QString& outputPath);
    
    /**
     * @brief Error occurred
     * @param message Error message
     */
    void error(const QString& message);
    
private slots:
    void onProcessOutput();
    void onProcessError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
private:
    QString findYtDlp() const;
    
    QProcess* m_process;
    QString m_ytdlpPath;
    QUrl m_currentUrl;
    QByteArray m_outputBuffer;
    QByteArray m_errorBuffer;
};

} // namespace OpenIDM

// Register metatypes for signal/slot
Q_DECLARE_METATYPE(OpenIDM::VideoInfo)
Q_DECLARE_METATYPE(OpenIDM::FormatInfo)
