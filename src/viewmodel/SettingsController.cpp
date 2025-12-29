/**
 * @file SettingsController.cpp
 * @brief Implementation of settings controller
 *
 * @copyright Copyright (c) 2024 OpenIDM Project
 * @license GPL-3.0-or-later
 */

#include "SettingsController.h"
#include "persistence/PersistenceManager.h"

namespace OpenIDM {

SettingsController::SettingsController(QObject* parent)
    : QObject(parent)
{
}

void SettingsController::setPersistenceManager(PersistenceManager* persistence)
{
    m_persistence = persistence;
    load();
}

void SettingsController::save()
{
    if (m_persistence) {
        m_persistence->saveSettings(m_settings);
    }
}

void SettingsController::load()
{
    if (m_persistence) {
        m_settings = m_persistence->loadSettings();
    }
}

} // namespace OpenIDM
