/**
 * @file PlatformIntegration.h
 * @brief Platform-specific integration (notifications, tray, etc.)
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_PLATFORMINTEGRATION_H
#define OPENIDM_PLATFORMINTEGRATION_H

#include <QObject>

namespace OpenIDM {

/**
 * @brief Platform-specific integrations
 */
class PlatformIntegration : public QObject {
    Q_OBJECT

public:
    explicit PlatformIntegration(QObject* parent = nullptr);

    /**
     * @brief Show system notification
     */
    void showNotification(const QString& title, const QString& message);

    /**
     * @brief Check if system tray is available
     */
    bool isSystemTrayAvailable() const;

    /**
     * @brief Minimize to system tray
     */
    void minimizeToTray();

private:
    // Platform-specific implementation
};

} // namespace OpenIDM

#endif // OPENIDM_PLATFORMINTEGRATION_H
