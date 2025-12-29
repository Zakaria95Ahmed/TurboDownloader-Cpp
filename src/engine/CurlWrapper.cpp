/**
 * @file CurlWrapper.cpp
 * @brief Implementation of RAII libcurl wrapper
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#include "CurlWrapper.h"

#include <QDebug>
#include <QRegularExpression>
#include <QMutexLocker>

#include <mutex>

namespace OpenIDM {

// ═══════════════════════════════════════════════════════════════════════════════
// CurlGlobalInit Implementation
// ═══════════════════════════════════════════════════════════════════════════════

CurlGlobalInit& CurlGlobalInit::instance()
{
    static CurlGlobalInit instance;
    return instance;
}

CurlGlobalInit::CurlGlobalInit()
{
    CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
    m_valid = (result == CURLE_OK);

    if (!m_valid) {
        qCritical() << "Failed to initialize libcurl:" << curl_easy_strerror(result);
    } else {
        qInfo() << "libcurl initialized:" << version();
    }
}

CurlGlobalInit::~CurlGlobalInit()
{
    if (m_valid) {
        curl_global_cleanup();
        qDebug() << "libcurl global cleanup complete";
    }
}

QString CurlGlobalInit::version() const
{
    return QString::fromUtf8(curl_version());
}

bool CurlGlobalInit::supportsProtocol(const QString& protocol) const
{
    curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    if (!info || !info->protocols) {
        return false;
    }

    QByteArray proto = protocol.toLower().toUtf8();
    for (const char* const* p = info->protocols; *p; ++p) {
        if (proto == *p) {
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CurlEasyHandle Implementation
// ═══════════════════════════════════════════════════════════════════════════════

CurlEasyHandle::CurlEasyHandle(QObject* parent)
    : QObject(parent)
{
    // Ensure global init
    CurlGlobalInit::instance();

    m_handle = curl_easy_init();
    if (!m_handle) {
        qCritical() << "Failed to create CURL easy handle";
        return;
    }

    // Set error buffer
    curl_easy_setopt(m_handle, CURLOPT_ERRORBUFFER, m_errorBuffer);

    // Default settings
    curl_easy_setopt(m_handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(m_handle, CURLOPT_TCP_KEEPIDLE, 60L);
    curl_easy_setopt(m_handle, CURLOPT_TCP_KEEPINTVL, 30L);

    // SSL verification (enabled by default)
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYHOST, 2L);

    // Default timeouts
    setConnectTimeout(Config::CONNECTION_TIMEOUT_SECONDS);
    setLowSpeedLimit(Config::LOW_SPEED_LIMIT_BYTES, Config::LOW_SPEED_TIME_SECONDS);

    // Default user agent
    setUserAgent("OpenIDM/1.0 (compatible; libcurl)");

    // Follow redirects by default
    setFollowRedirects(true);

    qDebug() << "CurlEasyHandle created";
}

CurlEasyHandle::~CurlEasyHandle()
{
    if (m_headerList) {
        curl_slist_free_all(m_headerList);
        m_headerList = nullptr;
    }

    if (m_handle) {
        curl_easy_cleanup(m_handle);
        m_handle = nullptr;
    }

    qDebug() << "CurlEasyHandle destroyed";
}

CurlEasyHandle::CurlEasyHandle(CurlEasyHandle&& other) noexcept
    : QObject(other.parent())
    , m_handle(std::exchange(other.m_handle, nullptr))
    , m_headerList(std::exchange(other.m_headerList, nullptr))
    , m_writeCallback(std::move(other.m_writeCallback))
    , m_progressCallback(std::move(other.m_progressCallback))
    , m_headerCallback(std::move(other.m_headerCallback))
    , m_aborted(other.m_aborted.load())
    , m_lastError(std::move(other.m_lastError))
    , m_headerInfo(std::move(other.m_headerInfo))
{
    std::memcpy(m_errorBuffer, other.m_errorBuffer, CURL_ERROR_SIZE);
}

CurlEasyHandle& CurlEasyHandle::operator=(CurlEasyHandle&& other) noexcept
{
    if (this != &other) {
        if (m_headerList) curl_slist_free_all(m_headerList);
        if (m_handle) curl_easy_cleanup(m_handle);

        m_handle = std::exchange(other.m_handle, nullptr);
        m_headerList = std::exchange(other.m_headerList, nullptr);
        m_writeCallback = std::move(other.m_writeCallback);
        m_progressCallback = std::move(other.m_progressCallback);
        m_headerCallback = std::move(other.m_headerCallback);
        m_aborted.store(other.m_aborted.load());
        m_lastError = std::move(other.m_lastError);
        m_headerInfo = std::move(other.m_headerInfo);
        std::memcpy(m_errorBuffer, other.m_errorBuffer, CURL_ERROR_SIZE);
    }
    return *this;
}

void CurlEasyHandle::reset()
{
    QMutexLocker locker(&m_mutex);

    if (!m_handle) return;

    curl_easy_reset(m_handle);
    m_aborted.store(false);
    m_lastError.clear();
    m_headerInfo = HttpHeaderInfo{};
    m_errorBuffer[0] = '\0';

    if (m_headerList) {
        curl_slist_free_all(m_headerList);
        m_headerList = nullptr;
    }

    // Restore default settings
    curl_easy_setopt(m_handle, CURLOPT_ERRORBUFFER, m_errorBuffer);
    curl_easy_setopt(m_handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    setConnectTimeout(Config::CONNECTION_TIMEOUT_SECONDS);
    setUserAgent("OpenIDM/1.0 (compatible; libcurl)");
    setFollowRedirects(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration Methods
// ═══════════════════════════════════════════════════════════════════════════════

void CurlEasyHandle::setUrl(const QUrl& url)
{
    if (!m_handle) return;
    QByteArray urlBytes = url.toString(QUrl::FullyEncoded).toUtf8();
    curl_easy_setopt(m_handle, CURLOPT_URL, urlBytes.constData());
}

void CurlEasyHandle::setRange(qint64 start, qint64 end)
{
    if (!m_handle) return;

    QString range;
    if (end >= 0) {
        range = QString("%1-%2").arg(start).arg(end);
    } else {
        range = QString("%1-").arg(start);
    }

    QByteArray rangeBytes = range.toUtf8();
    curl_easy_setopt(m_handle, CURLOPT_RANGE, rangeBytes.constData());
}

void CurlEasyHandle::clearRange()
{
    if (!m_handle) return;
    curl_easy_setopt(m_handle, CURLOPT_RANGE, nullptr);
}

void CurlEasyHandle::setConnectTimeout(int seconds)
{
    if (!m_handle) return;
    curl_easy_setopt(m_handle, CURLOPT_CONNECTTIMEOUT, static_cast<long>(seconds));
}

void CurlEasyHandle::setLowSpeedLimit(int bytesPerSecond, int seconds)
{
    if (!m_handle) return;
    curl_easy_setopt(m_handle, CURLOPT_LOW_SPEED_LIMIT, static_cast<long>(bytesPerSecond));
    curl_easy_setopt(m_handle, CURLOPT_LOW_SPEED_TIME, static_cast<long>(seconds));
}

void CurlEasyHandle::setMaxSpeed(qint64 bytesPerSecond)
{
    if (!m_handle) return;
    curl_easy_setopt(m_handle, CURLOPT_MAX_RECV_SPEED_LARGE, static_cast<curl_off_t>(bytesPerSecond));
}

void CurlEasyHandle::setUserAgent(const QString& userAgent)
{
    if (!m_handle) return;
    QByteArray ua = userAgent.toUtf8();
    curl_easy_setopt(m_handle, CURLOPT_USERAGENT, ua.constData());
}

void CurlEasyHandle::setReferer(const QString& referer)
{
    if (!m_handle) return;
    QByteArray ref = referer.toUtf8();
    curl_easy_setopt(m_handle, CURLOPT_REFERER, ref.constData());
}

void CurlEasyHandle::addHeader(const QString& header)
{
    QByteArray h = header.toUtf8();
    m_headerList = curl_slist_append(m_headerList, h.constData());
    curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, m_headerList);
}

void CurlEasyHandle::clearHeaders()
{
    if (m_headerList) {
        curl_slist_free_all(m_headerList);
        m_headerList = nullptr;
    }
    curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, nullptr);
}

void CurlEasyHandle::setSslVerification(bool verify)
{
    if (!m_handle) return;
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER, verify ? 1L : 0L);
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYHOST, verify ? 2L : 0L);
}

void CurlEasyHandle::setProxy(const QString& host, int port,
                               const QString& user, const QString& password)
{
    if (!m_handle) return;

    QString proxyUrl = QString("%1:%2").arg(host).arg(port);
    QByteArray proxy = proxyUrl.toUtf8();
    curl_easy_setopt(m_handle, CURLOPT_PROXY, proxy.constData());

    if (!user.isEmpty()) {
        QString userPwd = user;
        if (!password.isEmpty()) {
            userPwd += ":" + password;
        }
        QByteArray creds = userPwd.toUtf8();
        curl_easy_setopt(m_handle, CURLOPT_PROXYUSERPWD, creds.constData());
    }
}

void CurlEasyHandle::clearProxy()
{
    if (!m_handle) return;
    curl_easy_setopt(m_handle, CURLOPT_PROXY, nullptr);
    curl_easy_setopt(m_handle, CURLOPT_PROXYUSERPWD, nullptr);
}

void CurlEasyHandle::setFollowRedirects(bool follow, int maxRedirects)
{
    if (!m_handle) return;
    curl_easy_setopt(m_handle, CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L);
    curl_easy_setopt(m_handle, CURLOPT_MAXREDIRS, static_cast<long>(maxRedirects));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

void CurlEasyHandle::setWriteCallback(WriteCallback callback)
{
    m_writeCallback = std::move(callback);

    if (m_writeCallback) {
        curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, &CurlEasyHandle::writeCallbackStatic);
        curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, this);
    } else {
        curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, nullptr);
        curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, nullptr);
    }
}

void CurlEasyHandle::setProgressCallback(ProgressCallback callback)
{
    m_progressCallback = std::move(callback);

    if (m_progressCallback) {
        curl_easy_setopt(m_handle, CURLOPT_XFERINFOFUNCTION, &CurlEasyHandle::progressCallbackStatic);
        curl_easy_setopt(m_handle, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(m_handle, CURLOPT_NOPROGRESS, 0L);
    } else {
        curl_easy_setopt(m_handle, CURLOPT_XFERINFOFUNCTION, nullptr);
        curl_easy_setopt(m_handle, CURLOPT_XFERINFODATA, nullptr);
        curl_easy_setopt(m_handle, CURLOPT_NOPROGRESS, 1L);
    }
}

void CurlEasyHandle::setHeaderCallback(HeaderCallback callback)
{
    m_headerCallback = std::move(callback);

    // Always set our internal header handler for parsing
    curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, &CurlEasyHandle::headerCallbackStatic);
    curl_easy_setopt(m_handle, CURLOPT_HEADERDATA, this);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Static Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

size_t CurlEasyHandle::writeCallbackStatic(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* self = static_cast<CurlEasyHandle*>(userdata);
    size_t totalSize = size * nmemb;

    if (self->m_aborted.load()) {
        return 0; // Abort transfer
    }

    if (self->m_writeCallback) {
        std::span<const char> data(ptr, totalSize);
        bool shouldContinue = self->m_writeCallback(data);
        if (!shouldContinue) {
            return 0;
        }
    }

    emit self->dataReceived(static_cast<qint64>(totalSize));
    return totalSize;
}

int CurlEasyHandle::progressCallbackStatic(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                            curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
    auto* self = static_cast<CurlEasyHandle*>(clientp);

    if (self->m_aborted.load()) {
        return 1; // Abort transfer
    }

    if (self->m_progressCallback) {
        bool shouldContinue = self->m_progressCallback(
            static_cast<qint64>(dltotal),
            static_cast<qint64>(dlnow)
        );
        if (!shouldContinue) {
            return 1;
        }
    }

    emit self->progressUpdated(static_cast<qint64>(dltotal), static_cast<qint64>(dlnow));
    return 0;
}

size_t CurlEasyHandle::headerCallbackStatic(char* buffer, size_t size, size_t nitems, void* userdata)
{
    auto* self = static_cast<CurlEasyHandle*>(userdata);
    size_t totalSize = size * nitems;

    QString line = QString::fromUtf8(buffer, static_cast<int>(totalSize)).trimmed();
    self->parseHeaderLine(line);

    if (self->m_headerCallback) {
        self->m_headerCallback(line);
    }

    return totalSize;
}

void CurlEasyHandle::parseHeaderLine(const QString& line)
{
    if (line.isEmpty()) return;

    // Parse Content-Length
    if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
        QString value = line.mid(15).trimmed();
        bool ok;
        qint64 length = value.toLongLong(&ok);
        if (ok) {
            m_headerInfo.contentLength = length;
        }
    }
    // Parse Accept-Ranges
    else if (line.startsWith("Accept-Ranges:", Qt::CaseInsensitive)) {
        QString value = line.mid(14).trimmed();
        m_headerInfo.acceptRanges = (value.toLower() == "bytes");
    }
    // Parse Content-Type
    else if (line.startsWith("Content-Type:", Qt::CaseInsensitive)) {
        m_headerInfo.contentType = line.mid(13).trimmed();
    }
    // Parse Content-Disposition
    else if (line.startsWith("Content-Disposition:", Qt::CaseInsensitive)) {
        m_headerInfo.contentDisposition = line.mid(20).trimmed();
    }
    // Parse ETag
    else if (line.startsWith("ETag:", Qt::CaseInsensitive)) {
        m_headerInfo.etag = line.mid(5).trimmed();
    }
    // Parse Last-Modified
    else if (line.startsWith("Last-Modified:", Qt::CaseInsensitive)) {
        QString dateStr = line.mid(14).trimmed();
        m_headerInfo.lastModified = QDateTime::fromString(dateStr, Qt::RFC2822Date);
    }
    // Parse Server
    else if (line.startsWith("Server:", Qt::CaseInsensitive)) {
        m_headerInfo.server = line.mid(7).trimmed();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Execution
// ═══════════════════════════════════════════════════════════════════════════════

CurlResult CurlEasyHandle::performHead()
{
    if (!m_handle) {
        return CurlResult::fromError(CURLE_FAILED_INIT, "Handle not initialized");
    }

    m_aborted.store(false);
    m_errorBuffer[0] = '\0';
    m_headerInfo = HttpHeaderInfo{};

    curl_easy_setopt(m_handle, CURLOPT_NOBODY, 1L);

    // Set up header callback
    curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, &CurlEasyHandle::headerCallbackStatic);
    curl_easy_setopt(m_handle, CURLOPT_HEADERDATA, this);

    CURLcode code = curl_easy_perform(m_handle);

    CurlResult result;
    result.code = code;

    if (code == CURLE_OK) {
        curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &result.httpCode);
        result.totalBytes = m_headerInfo.contentLength;
    } else {
        result.errorMessage = QString::fromUtf8(m_errorBuffer[0] ?
                                                m_errorBuffer : curl_easy_strerror(code));
        m_lastError = result.errorMessage;
        emit errorOccurred(result.errorMessage);
    }

    // Reset to GET for subsequent calls
    curl_easy_setopt(m_handle, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(m_handle, CURLOPT_HTTPGET, 1L);

    return result;
}

CurlResult CurlEasyHandle::performGet()
{
    if (!m_handle) {
        return CurlResult::fromError(CURLE_FAILED_INIT, "Handle not initialized");
    }

    m_aborted.store(false);
    m_errorBuffer[0] = '\0';
    m_headerInfo = HttpHeaderInfo{};

    curl_easy_setopt(m_handle, CURLOPT_HTTPGET, 1L);

    // Set up header callback
    curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, &CurlEasyHandle::headerCallbackStatic);
    curl_easy_setopt(m_handle, CURLOPT_HEADERDATA, this);

    CURLcode code = curl_easy_perform(m_handle);

    CurlResult result;
    result.code = code;

    if (code == CURLE_OK || code == CURLE_PARTIAL_FILE) {
        curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &result.httpCode);

        double downloaded = 0;
        curl_easy_getinfo(m_handle, CURLINFO_SIZE_DOWNLOAD, &downloaded);
        result.bytesDownloaded = static_cast<qint64>(downloaded);
        result.totalBytes = m_headerInfo.contentLength;
    } else if (code == CURLE_ABORTED_BY_CALLBACK) {
        result.errorMessage = "Aborted";
    } else {
        result.errorMessage = QString::fromUtf8(m_errorBuffer[0] ?
                                                m_errorBuffer : curl_easy_strerror(code));
        m_lastError = result.errorMessage;
        emit errorOccurred(result.errorMessage);
    }

    return result;
}

void CurlEasyHandle::abort()
{
    m_aborted.store(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Info Retrieval
// ═══════════════════════════════════════════════════════════════════════════════

long CurlEasyHandle::responseCode() const
{
    if (!m_handle) return 0;

    long code = 0;
    curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &code);
    return code;
}

QString CurlEasyHandle::effectiveUrl() const
{
    if (!m_handle) return QString();

    char* url = nullptr;
    curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &url);
    return url ? QString::fromUtf8(url) : QString();
}

qint64 CurlEasyHandle::contentLength() const
{
    if (!m_handle) return -1;

    curl_off_t length = -1;
    curl_easy_getinfo(m_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
    return static_cast<qint64>(length);
}

QString CurlEasyHandle::contentType() const
{
    if (!m_handle) return QString();

    char* type = nullptr;
    curl_easy_getinfo(m_handle, CURLINFO_CONTENT_TYPE, &type);
    return type ? QString::fromUtf8(type) : QString();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Utility Functions
// ═══════════════════════════════════════════════════════════════════════════════

std::optional<HttpHeaderInfo> fetchHeaders(const QUrl& url)
{
    CurlEasyHandle handle;
    if (!handle.isValid()) {
        return std::nullopt;
    }

    handle.setUrl(url);
    CurlResult result = handle.performHead();

    if (result.success()) {
        return handle.headerInfo();
    }

    return std::nullopt;
}

QString formatCurlError(CURLcode code)
{
    return QString::fromUtf8(curl_easy_strerror(code));
}

bool isDownloadableUrl(const QUrl& url)
{
    QString scheme = url.scheme().toLower();
    return scheme == "http" || scheme == "https" || scheme == "ftp" || scheme == "ftps";
}

} // namespace OpenIDM
