/**
 * @file StreamParser.cpp
 * @brief Parser for HLS/M3U8 and other streaming formats
 */

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QRegularExpression>
#include <vector>

namespace OpenIDM {

/**
 * @brief Represents a segment in an HLS stream
 */
struct HlsSegment {
    QString url;
    double duration;
    QString title;
    int sequenceNumber;
    bool isDiscontinuity;
};

/**
 * @brief Represents an HLS variant stream
 */
struct HlsVariant {
    QString url;
    int bandwidth;
    QString resolution;
    QString codecs;
    QString audio;
    QString subtitles;
};

/**
 * @brief Parsed HLS playlist
 */
struct HlsPlaylist {
    bool isMaster;
    int targetDuration;
    int mediaSequence;
    bool isEndList;
    std::vector<HlsVariant> variants;
    std::vector<HlsSegment> segments;
};

/**
 * @class StreamParser
 * @brief Parses HLS/M3U8 playlists
 */
class StreamParser {
public:
    /**
     * @brief Check if content is an M3U8 playlist
     * @param content Content to check
     * @return True if content is M3U8
     */
    static bool isM3U8(const QString& content) {
        return content.trimmed().startsWith(QStringLiteral("#EXTM3U"));
    }
    
    /**
     * @brief Parse an M3U8 playlist
     * @param content Playlist content
     * @param baseUrl Base URL for resolving relative URLs
     * @return Parsed playlist structure
     */
    static HlsPlaylist parseM3U8(const QString& content, const QUrl& baseUrl) {
        HlsPlaylist playlist;
        playlist.isMaster = false;
        playlist.targetDuration = 0;
        playlist.mediaSequence = 0;
        playlist.isEndList = false;
        
        QStringList lines = content.split(QRegularExpression(QStringLiteral("[\r\n]+")));
        
        double currentDuration = 0;
        QString currentTitle;
        bool expectSegment = false;
        int sequenceNumber = 0;
        
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            
            if (trimmed.isEmpty() || trimmed == QStringLiteral("#EXTM3U")) {
                continue;
            }
            
            // Master playlist variant
            if (trimmed.startsWith(QStringLiteral("#EXT-X-STREAM-INF:"))) {
                playlist.isMaster = true;
                
                HlsVariant variant;
                
                // Parse attributes
                static QRegularExpression bandwidthRegex(QStringLiteral("BANDWIDTH=(\\d+)"));
                static QRegularExpression resolutionRegex(QStringLiteral("RESOLUTION=([\\dx]+)"));
                static QRegularExpression codecsRegex(QStringLiteral("CODECS=\"([^\"]+)\""));
                
                auto match = bandwidthRegex.match(trimmed);
                if (match.hasMatch()) {
                    variant.bandwidth = match.captured(1).toInt();
                }
                
                match = resolutionRegex.match(trimmed);
                if (match.hasMatch()) {
                    variant.resolution = match.captured(1);
                }
                
                match = codecsRegex.match(trimmed);
                if (match.hasMatch()) {
                    variant.codecs = match.captured(1);
                }
                
                playlist.variants.push_back(variant);
                continue;
            }
            
            // Target duration
            if (trimmed.startsWith(QStringLiteral("#EXT-X-TARGETDURATION:"))) {
                playlist.targetDuration = trimmed.mid(22).toInt();
                continue;
            }
            
            // Media sequence
            if (trimmed.startsWith(QStringLiteral("#EXT-X-MEDIA-SEQUENCE:"))) {
                playlist.mediaSequence = trimmed.mid(22).toInt();
                sequenceNumber = playlist.mediaSequence;
                continue;
            }
            
            // End list marker
            if (trimmed == QStringLiteral("#EXT-X-ENDLIST")) {
                playlist.isEndList = true;
                continue;
            }
            
            // Segment info
            if (trimmed.startsWith(QStringLiteral("#EXTINF:"))) {
                QString info = trimmed.mid(8);
                int commaPos = info.indexOf(',');
                if (commaPos > 0) {
                    currentDuration = info.left(commaPos).toDouble();
                    currentTitle = info.mid(commaPos + 1).trimmed();
                } else {
                    currentDuration = info.toDouble();
                }
                expectSegment = true;
                continue;
            }
            
            // Discontinuity marker
            if (trimmed == QStringLiteral("#EXT-X-DISCONTINUITY")) {
                continue;
            }
            
            // URL line (segment or variant)
            if (!trimmed.startsWith('#')) {
                QString resolvedUrl = resolveUrl(trimmed, baseUrl);
                
                if (playlist.isMaster && !playlist.variants.empty()) {
                    // This is a variant URL
                    playlist.variants.back().url = resolvedUrl;
                } else if (expectSegment) {
                    // This is a segment URL
                    HlsSegment segment;
                    segment.url = resolvedUrl;
                    segment.duration = currentDuration;
                    segment.title = currentTitle;
                    segment.sequenceNumber = sequenceNumber++;
                    segment.isDiscontinuity = false;
                    
                    playlist.segments.push_back(segment);
                    expectSegment = false;
                }
            }
        }
        
        return playlist;
    }
    
    /**
     * @brief Get total duration of all segments
     * @param playlist Parsed playlist
     * @return Total duration in seconds
     */
    static double totalDuration(const HlsPlaylist& playlist) {
        double total = 0;
        for (const auto& segment : playlist.segments) {
            total += segment.duration;
        }
        return total;
    }
    
    /**
     * @brief Get best quality variant
     * @param playlist Master playlist
     * @return Variant with highest bandwidth
     */
    static const HlsVariant* bestVariant(const HlsPlaylist& playlist) {
        if (playlist.variants.empty()) {
            return nullptr;
        }
        
        const HlsVariant* best = &playlist.variants[0];
        for (const auto& variant : playlist.variants) {
            if (variant.bandwidth > best->bandwidth) {
                best = &variant;
            }
        }
        return best;
    }
    
private:
    /**
     * @brief Resolve a potentially relative URL
     * @param url URL to resolve
     * @param baseUrl Base URL for resolution
     * @return Absolute URL
     */
    static QString resolveUrl(const QString& url, const QUrl& baseUrl) {
        if (url.startsWith(QStringLiteral("http://")) || 
            url.startsWith(QStringLiteral("https://"))) {
            return url;
        }
        
        return baseUrl.resolved(QUrl(url)).toString();
    }
};

} // namespace OpenIDM
