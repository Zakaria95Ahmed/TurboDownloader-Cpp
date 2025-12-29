/**
 * @file StreamingResolver.h
 * @brief URL resolver for streaming services
 *
 * Wraps yt-dlp to resolve streaming URLs from YouTube, Vimeo, etc.
 * into direct download URLs.
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_STREAMINGRESOLVER_H
#define OPENIDM_STREAMINGRESOLVER_H

#include <QObject>
#include <QUrl>
#include <QProcess>

#include <memory>

namespace OpenIDM {

/**
 * @brief Information about a resolved stream
 */
struct StreamInfo {
    QUrl url;                    ///< Direct download URL
    QString title;               ///< Video/audio title
    QString extension;           ///< File extension
    QString format;              ///< Format description
    qint64 fileSize = -1;        ///< File size if known
    int duration = 0;            ///< Duration in seconds
    QString thumbnail;           ///< Thumbnail URL
    QString description;         ///< Video description
    QString uploader;            ///< Channel/uploader name
};

/**
 * @brief Resolves streaming URLs using yt-dlp
 *
 * This class provides async URL resolution for streaming services.
 * It automatically detects and uses yt-dlp if available.
 */
class StreamingResolver : public QObject {
    Q_OBJECT

public:
    explicit StreamingResolver(QObject* parent = nullptr);
    ~StreamingResolver() override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Set custom path to yt-dlp executable
     */
    void setYtDlpPath(const QString& path);

    /**
     * @brief Get current yt-dlp path
     */
    [[nodiscard]] QString ytDlpPath() const { return m_ytdlpPath; }

    /**
     * @brief Set preferred format (e.g., "best", "bestvideo+bestaudio")
     */
    void setPreferredFormat(const QString& format);

    /**
     * @brief Set preferred quality (e.g., "1080", "720", "best")
     */
    void setPreferredQuality(const QString& quality);

    // ═══════════════════════════════════════════════════════════════════════════
    // Status
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Check if yt-dlp is available
     */
    [[nodiscard]] bool isAvailable() const;

    /**
     * @brief Get yt-dlp version
     */
    [[nodiscard]] QString version() const;

    /**
     * @brief Check if URL is a supported streaming service
     */
    [[nodiscard]] bool isSupportedUrl(const QUrl& url) const;

    /**
     * @brief Check if resolution is in progress
     */
    [[nodiscard]] bool isResolving() const { return m_process != nullptr; }

    // ═══════════════════════════════════════════════════════════════════════════
    // Resolution
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Resolve a streaming URL (async)
     *
     * Emits resolved() on success, error() on failure.
     */
    void resolve(const QUrl& url);

    /**
     * @brief Cancel current resolution
     */
    void cancel();

signals:
    /**
     * @brief Emitted when URL is resolved successfully
     */
    void resolved(const QUrl& directUrl);

    /**
     * @brief Emitted with full stream information
     */
    void resolvedWithInfo(const StreamInfo& info);

    /**
     * @brief Emitted on error
     */
    void error(const QString& message);

    /**
     * @brief Emitted with progress updates
     */
    void progress(const QString& status);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onProcessReadyRead();

private:
    QString findYtDlp() const;
    void parseOutput(const QByteArray& output);

    QString m_ytdlpPath;
    QString m_preferredFormat = "best";
    QString m_preferredQuality = "best";

    QProcess* m_process = nullptr;
    QUrl m_currentUrl;
    QByteArray m_outputBuffer;
};

} // namespace OpenIDM

#endif // OPENIDM_STREAMINGRESOLVER_H
