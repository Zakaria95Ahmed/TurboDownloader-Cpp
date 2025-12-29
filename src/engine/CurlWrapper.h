/**
 * @file CurlWrapper.h
 * @brief RAII wrapper for libcurl with modern C++ interface
 *
 * Provides a clean, exception-safe interface to libcurl that handles:
 * - Global initialization/cleanup
 * - Easy handle lifecycle
 * - Progress callbacks
 * - HTTP headers
 * - Error handling
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_CURLWRAPPER_H
#define OPENIDM_CURLWRAPPER_H

#include <curl/curl.h>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QByteArray>
#include <QMutex>

#include <memory>
#include <functional>
#include <optional>
#include <span>

#include "DownloadTypes.h"

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// Forward Declarations
// ═══════════════════════════════════════════════════════════════════════════════

class CurlGlobalInit;
class CurlEasyHandle;

// ═══════════════════════════════════════════════════════════════════════════════
// Result Types
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Result of a CURL operation
 */
struct CurlResult {
    CURLcode code = CURLE_OK;
    long httpCode = 0;
    QString errorMessage;
    qint64 bytesDownloaded = 0;
    qint64 totalBytes = -1;

    [[nodiscard]] bool success() const noexcept {
        return code == CURLE_OK && httpCode >= 200 && httpCode < 400;
    }

    [[nodiscard]] bool isPartialContent() const noexcept {
        return httpCode == 206;
    }

    [[nodiscard]] bool isRangeNotSatisfiable() const noexcept {
        return httpCode == 416;
    }

    [[nodiscard]] static CurlResult fromError(CURLcode c, const char* msg) {
        return CurlResult{c, 0, QString::fromUtf8(msg), 0, -1};
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Callback Types
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Write callback - called when data is received
 * @param data Received data span
 * @return true to continue, false to abort
 */
using WriteCallback = std::function<bool(std::span<const char> data)>;

/**
 * @brief Progress callback - called periodically during transfer
 * @param downloadTotal Total bytes to download
 * @param downloadNow Bytes downloaded so far
 * @return true to continue, false to abort
 */
using ProgressCallback = std::function<bool(qint64 downloadTotal, qint64 downloadNow)>;

/**
 * @brief Header callback - called for each HTTP header line
 * @param headerLine The complete header line
 */
using HeaderCallback = std::function<void(const QString& headerLine)>;

// ═══════════════════════════════════════════════════════════════════════════════
// CurlGlobalInit - RAII for global init/cleanup
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Manages global libcurl initialization
 *
 * Use via instance() to ensure single initialization.
 * Thread-safe via std::call_once.
 */
class CurlGlobalInit {
public:
    /**
     * @brief Get singleton instance (initializes on first call)
     */
    static CurlGlobalInit& instance();

    ~CurlGlobalInit();

    // Non-copyable, non-movable
    CurlGlobalInit(const CurlGlobalInit&) = delete;
    CurlGlobalInit& operator=(const CurlGlobalInit&) = delete;
    CurlGlobalInit(CurlGlobalInit&&) = delete;
    CurlGlobalInit& operator=(CurlGlobalInit&&) = delete;

    /**
     * @brief Check if initialization succeeded
     */
    [[nodiscard]] bool isValid() const noexcept { return m_valid; }

    /**
     * @brief Get libcurl version string
     */
    [[nodiscard]] QString version() const;

    /**
     * @brief Check for specific protocol support
     */
    [[nodiscard]] bool supportsProtocol(const QString& protocol) const;

private:
    CurlGlobalInit();
    bool m_valid = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// CurlEasyHandle - RAII wrapper for CURL easy handle
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief RAII wrapper for a libcurl easy handle
 *
 * Each SegmentWorker owns one CurlEasyHandle for its downloads.
 * Handles are reusable via reset() for connection reuse benefits.
 */
class CurlEasyHandle : public QObject {
    Q_OBJECT

public:
    explicit CurlEasyHandle(QObject* parent = nullptr);
    ~CurlEasyHandle() override;

    // Non-copyable
    CurlEasyHandle(const CurlEasyHandle&) = delete;
    CurlEasyHandle& operator=(const CurlEasyHandle&) = delete;

    // Movable
    CurlEasyHandle(CurlEasyHandle&& other) noexcept;
    CurlEasyHandle& operator=(CurlEasyHandle&& other) noexcept;

    /**
     * @brief Check if handle is valid
     */
    [[nodiscard]] bool isValid() const noexcept { return m_handle != nullptr; }

    /**
     * @brief Reset handle for reuse (keeps connection pool)
     */
    void reset();

    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Set the URL to download
     */
    void setUrl(const QUrl& url);

    /**
     * @brief Set byte range for partial download
     * @param start Start byte (inclusive)
     * @param end End byte (inclusive), -1 for remainder
     */
    void setRange(qint64 start, qint64 end = -1);

    /**
     * @brief Clear any range setting
     */
    void clearRange();

    /**
     * @brief Set connection timeout
     */
    void setConnectTimeout(int seconds);

    /**
     * @brief Set low speed abort threshold
     */
    void setLowSpeedLimit(int bytesPerSecond, int seconds);

    /**
     * @brief Set maximum download speed
     */
    void setMaxSpeed(qint64 bytesPerSecond);

    /**
     * @brief Set HTTP user agent
     */
    void setUserAgent(const QString& userAgent);

    /**
     * @brief Set HTTP referer
     */
    void setReferer(const QString& referer);

    /**
     * @brief Add custom HTTP header
     */
    void addHeader(const QString& header);

    /**
     * @brief Clear custom headers
     */
    void clearHeaders();

    /**
     * @brief Enable/disable SSL verification
     */
    void setSslVerification(bool verify);

    /**
     * @brief Set proxy configuration
     */
    void setProxy(const QString& host, int port,
                  const QString& user = QString(),
                  const QString& password = QString());

    /**
     * @brief Disable proxy
     */
    void clearProxy();

    /**
     * @brief Follow HTTP redirects
     */
    void setFollowRedirects(bool follow, int maxRedirects = 10);

    // ═══════════════════════════════════════════════════════════════════════════
    // Callbacks
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Set callback for received data
     */
    void setWriteCallback(WriteCallback callback);

    /**
     * @brief Set callback for progress updates
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set callback for HTTP headers
     */
    void setHeaderCallback(HeaderCallback callback);

    // ═══════════════════════════════════════════════════════════════════════════
    // Execution
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Perform a HEAD request (metadata only)
     */
    [[nodiscard]] CurlResult performHead();

    /**
     * @brief Perform a GET request (download)
     */
    [[nodiscard]] CurlResult performGet();

    /**
     * @brief Abort an ongoing request
     *
     * Thread-safe - can be called from any thread.
     */
    void abort();

    /**
     * @brief Check if abort was requested
     */
    [[nodiscard]] bool isAborted() const noexcept { return m_aborted.load(); }

    // ═══════════════════════════════════════════════════════════════════════════
    // Info Retrieval
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Get HTTP response code
     */
    [[nodiscard]] long responseCode() const;

    /**
     * @brief Get effective URL after redirects
     */
    [[nodiscard]] QString effectiveUrl() const;

    /**
     * @brief Get content length from response
     */
    [[nodiscard]] qint64 contentLength() const;

    /**
     * @brief Get content type
     */
    [[nodiscard]] QString contentType() const;

    /**
     * @brief Parse HTTP headers into structured info
     */
    [[nodiscard]] HttpHeaderInfo headerInfo() const { return m_headerInfo; }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] QString lastError() const { return m_lastError; }

signals:
    /**
     * @brief Emitted when data is received
     */
    void dataReceived(qint64 bytes);

    /**
     * @brief Emitted on progress update
     */
    void progressUpdated(qint64 downloadTotal, qint64 downloadNow);

    /**
     * @brief Emitted on error
     */
    void errorOccurred(const QString& error);

private:
    // Static callbacks for libcurl
    static size_t writeCallbackStatic(char* ptr, size_t size, size_t nmemb, void* userdata);
    static int progressCallbackStatic(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                      curl_off_t ultotal, curl_off_t ulnow);
    static size_t headerCallbackStatic(char* buffer, size_t size, size_t nitems, void* userdata);

    void parseHeaderLine(const QString& line);

    CURL* m_handle = nullptr;
    curl_slist* m_headerList = nullptr;
    char m_errorBuffer[CURL_ERROR_SIZE] = {0};

    WriteCallback m_writeCallback;
    ProgressCallback m_progressCallback;
    HeaderCallback m_headerCallback;

    std::atomic<bool> m_aborted{false};
    QString m_lastError;
    HttpHeaderInfo m_headerInfo;

    mutable QMutex m_mutex;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Utility Functions
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Fetch HTTP headers for a URL
 * @param url The URL to query
 * @return HttpHeaderInfo with server response details
 */
[[nodiscard]] std::optional<HttpHeaderInfo> fetchHeaders(const QUrl& url);

/**
 * @brief Format curl error for display
 */
[[nodiscard]] QString formatCurlError(CURLcode code);

/**
 * @brief Check if URL is likely downloadable
 */
[[nodiscard]] bool isDownloadableUrl(const QUrl& url);

} // namespace OpenIDM

#endif // OPENIDM_CURLWRAPPER_H
