/**
 * @file NotificationManager.h
 * @brief Cross-platform notification system
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_NOTIFICATIONMANAGER_H
#define OPENIDM_NOTIFICATIONMANAGER_H

#include <QObject>

namespace OpenIDM {

/**
 * @brief Manages system notifications
 */
class NotificationManager : public QObject {
    Q_OBJECT

public:
    explicit NotificationManager(QObject* parent = nullptr);

    void notifyDownloadComplete(const QString& fileName);
    void notifyDownloadError(const QString& fileName, const QString& error);
    void notifyQueueComplete();
};

} // namespace OpenIDM

#endif // OPENIDM_NOTIFICATIONMANAGER_H
