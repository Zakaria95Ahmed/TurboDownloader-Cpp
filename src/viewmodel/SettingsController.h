/**
 * @file SettingsController.h
 * @brief QML controller for settings management
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#ifndef OPENIDM_SETTINGSCONTROLLER_H
#define OPENIDM_SETTINGSCONTROLLER_H

#include <QObject>
#include "engine/DownloadTypes.h"

namespace OpenIDM {

class PersistenceManager;

/**
 * @brief Settings controller for QML
 */
class SettingsController : public QObject {
    Q_OBJECT

public:
    explicit SettingsController(QObject* parent = nullptr);

    void setPersistenceManager(PersistenceManager* persistence);

    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

private:
    PersistenceManager* m_persistence = nullptr;
    Settings m_settings;
};

} // namespace OpenIDM

#endif // OPENIDM_SETTINGSCONTROLLER_H
