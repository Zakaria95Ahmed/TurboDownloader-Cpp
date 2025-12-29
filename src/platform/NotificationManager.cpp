/**
 * @file NotificationManager.cpp
 * @brief Implementation of notification manager
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#include "NotificationManager.h"
#include <QDebug>

namespace OpenIDM {

NotificationManager::NotificationManager(QObject* parent)
    : QObject(parent)
{
}

void NotificationManager::notifyDownloadComplete(const QString& fileName)
{
    qDebug() << "Download complete:" << fileName;
    // TODO: Show system notification
}

void NotificationManager::notifyDownloadError(const QString& fileName, const QString& error)
{
    qDebug() << "Download error:" << fileName << "-" << error;
    // TODO: Show system notification
}

void NotificationManager::notifyQueueComplete()
{
    qDebug() << "All downloads complete";
    // TODO: Show system notification
}

} // namespace OpenIDM
