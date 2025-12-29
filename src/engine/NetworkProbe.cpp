/**
 * @file NetworkProbe.cpp
 * @brief Implementation of NetworkProbe - server capability detection
 */

#include "openidm/engine/NetworkProbe.h"
#include <curl/curl.h>
#include <QDebug>
#include <QRegularExpression>
#include <QtConcurrent>

namespace OpenIDM {

NetworkProbe::NetworkProbe(QObject* parent)
    : QObject(parent)
{
}

NetworkProbe::~NetworkProbe() {
    cancel();
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

void NetworkProbe::probe(const QUrl& url) {
    if (m_probing) {
        qWarning() << "NetworkProbe: Already probing";
        return;
    }
    
    m_probing = true;
    m_cancelled = false;
    m_capabilities = ServerCapabilities{};
    m_rawHeaders.clear();
    
    // Run probe in background thread
    QtConcurrent::run([this, url]() {
        performProbe(url);
    });
}

void NetworkProbe::cancel() {
    m_cancelled = true;
}

void NetworkProbe::performProbe(const QUrl& url) {
    qDebug() << "NetworkProbe: Probing" << url.toString();
    
    // Initialize curl
    m_curl = curl_easy_init();
    if (!m_curl) {
        DownloadError error;
        error.category = ErrorCategory::Unknown;
        error.message = QStringLiteral("Failed to initialize curl");
        
        QMetaObject::invokeMethod(this, [this, error]() {
            m_probing = false;
            emit failed(error);
        }, Qt::QueuedConnection);
        return;
    }
    
    QByteArray urlBytes = url.toString().toUtf8();
    
    // Configure curl for HEAD request
    curl_easy_setopt(m_curl, CURLOPT_URL, urlBytes.constData());
    curl_easy_setopt(m_curl, CURLOPT_NOBODY, 1L);  // HEAD request
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    
    // Timeouts
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 60L);
    
    // SSL
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
#ifdef Q_OS_WIN
    curl_easy_setopt(m_curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
    
    // Header callback
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, this);
    
    // User agent
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "OpenIDM/1.0");
    
    // Perform request
    CURLcode result = curl_easy_perform(m_curl);
    
    if (m_cancelled) {
        curl_easy_cleanup(m_curl);
        m_curl = nullptr;
        m_probing = false;
        return;
    }
    
    if (result != CURLE_OK) {
        DownloadError error;
        error.category = ErrorCategory::Network;
        error.errorCode = result;
        error.message = QString::fromUtf8(curl_easy_strerror(result));
        
        curl_easy_cleanup(m_curl);
        m_curl = nullptr;
        
        QMetaObject::invokeMethod(this, [this, error]() {
            m_probing = false;
            emit failed(error);
        }, Qt::QueuedConnection);
        return;
    }
    
    // Get HTTP response code
    long httpCode = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
    m_capabilities.httpStatusCode = static_cast<int>(httpCode);
    
    // Get content length
    curl_off_t contentLength = -1;
    curl_easy_getinfo(m_curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
    m_capabilities.contentLength = contentLength;
    
    // Get content type
    char* contentType = nullptr;
    curl_easy_getinfo(m_curl, CURLINFO_CONTENT_TYPE, &contentType);
    if (contentType) {
        m_capabilities.contentType = QString::fromUtf8(contentType);
    }
    
    // Parse headers for additional info
    parseHeaders();
    
    curl_easy_cleanup(m_curl);
    m_curl = nullptr;
    
    // Check for errors
    if (httpCode >= 400) {
        DownloadError error;
        error.category = (httpCode >= 500) ? ErrorCategory::ServerError : ErrorCategory::ClientError;
        error.errorCode = httpCode;
        error.message = QStringLiteral("HTTP error %1").arg(httpCode);
        
        QMetaObject::invokeMethod(this, [this, error]() {
            m_probing = false;
            emit failed(error);
        }, Qt::QueuedConnection);
        return;
    }
    
    qDebug() << "NetworkProbe: Completed. Size:" << m_capabilities.contentLength
             << "Ranges:" << m_capabilities.supportsRanges
             << "Type:" << m_capabilities.contentType;
    
    ServerCapabilities caps = m_capabilities;
    QMetaObject::invokeMethod(this, [this, caps]() {
        m_probing = false;
        emit completed(caps);
    }, Qt::QueuedConnection);
}

void NetworkProbe::parseHeaders() {
    // Parse Accept-Ranges
    static QRegularExpression acceptRangesRegex(
        QStringLiteral("Accept-Ranges:\\s*(bytes|none)", QStringLiteral("i").at(0)),
        QRegularExpression::CaseInsensitiveOption
    );
    
    auto match = acceptRangesRegex.match(m_rawHeaders);
    if (match.hasMatch()) {
        m_capabilities.supportsRanges = (match.captured(1).toLower() == QStringLiteral("bytes"));
    }
    
    // Parse Content-Disposition for filename
    static QRegularExpression contentDispositionRegex(
        QStringLiteral("Content-Disposition:.*filename\\*?=['\"]?([^'\"\\r\\n;]+)"),
        QRegularExpression::CaseInsensitiveOption
    );
    
    match = contentDispositionRegex.match(m_rawHeaders);
    if (match.hasMatch()) {
        QString filename = match.captured(1).trimmed();
        // Handle URL encoding
        filename = QUrl::fromPercentEncoding(filename.toUtf8());
        // Remove quotes if present
        if (filename.startsWith('"') && filename.endsWith('"')) {
            filename = filename.mid(1, filename.length() - 2);
        }
        m_capabilities.fileName = filename;
    }
    
    // Parse ETag
    static QRegularExpression etagRegex(
        QStringLiteral("ETag:\\s*\"?([^\"\\r\\n]+)\"?"),
        QRegularExpression::CaseInsensitiveOption
    );
    
    match = etagRegex.match(m_rawHeaders);
    if (match.hasMatch()) {
        m_capabilities.etag = match.captured(1).trimmed();
    }
    
    // Parse Last-Modified
    static QRegularExpression lastModifiedRegex(
        QStringLiteral("Last-Modified:\\s*([^\\r\\n]+)"),
        QRegularExpression::CaseInsensitiveOption
    );
    
    match = lastModifiedRegex.match(m_rawHeaders);
    if (match.hasMatch()) {
        m_capabilities.lastModified = match.captured(1).trimmed();
    }
    
    // Parse Content-Encoding
    static QRegularExpression contentEncodingRegex(
        QStringLiteral("Content-Encoding:\\s*([^\\r\\n]+)"),
        QRegularExpression::CaseInsensitiveOption
    );
    
    match = contentEncodingRegex.match(m_rawHeaders);
    if (match.hasMatch()) {
        m_capabilities.supportsCompression = true;
    }
}

size_t NetworkProbe::headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* probe = static_cast<NetworkProbe*>(userdata);
    size_t totalSize = size * nitems;
    
    probe->m_rawHeaders += QString::fromUtf8(buffer, totalSize);
    
    return totalSize;
}

} // namespace OpenIDM
