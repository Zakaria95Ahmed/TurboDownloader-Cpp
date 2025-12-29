/**
 * @file NetworkProbe.h
 * @brief Server capability detection via HTTP HEAD request
 */

#pragma once

#include "openidm/engine/Types.h"
#include <QObject>
#include <QUrl>

typedef void CURL;

namespace OpenIDM {

/**
 * @class NetworkProbe
 * @brief Probes a server to detect download capabilities
 * 
 * Performs a HEAD request to determine:
 * - File size (Content-Length)
 * - Range request support (Accept-Ranges)
 * - File name (Content-Disposition)
 * - Content type
 * - ETag and Last-Modified for resume validation
 */
class NetworkProbe : public QObject {
    Q_OBJECT

public:
    explicit NetworkProbe(QObject* parent = nullptr);
    ~NetworkProbe() override;
    
    /**
     * @brief Start probing a URL
     * @param url URL to probe
     */
    void probe(const QUrl& url);
    
    /**
     * @brief Cancel ongoing probe
     */
    void cancel();
    
    /**
     * @brief Check if probe is in progress
     */
    bool isProbing() const { return m_probing; }
    
signals:
    /**
     * @brief Emitted when probe completes successfully
     * @param capabilities Detected server capabilities
     */
    void completed(const ServerCapabilities& capabilities);
    
    /**
     * @brief Emitted when probe fails
     * @param error Error information
     */
    void failed(const DownloadError& error);
    
private:
    void performProbe(const QUrl& url);
    void parseHeaders();
    
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    
    CURL* m_curl{nullptr};
    bool m_probing{false};
    bool m_cancelled{false};
    
    ServerCapabilities m_capabilities;
    QString m_rawHeaders;
};

} // namespace OpenIDM
