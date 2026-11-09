// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

class QCheckBox;
class QComboBox;

/// Obsidium: "Старые настройки" - the old main window's commands that the drawn window has no
/// place for yet (stop the game, rename, the instance icon, accounts, help...), so that nothing
/// disappears while the new window is being built.
class LegacyPage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit LegacyPage(QWidget* parent = nullptr);
    ~LegacyPage() override = default;

    QString displayName() const override { return tr("Старые настройки"); }
    QIcon icon() const override { return QIcon::fromTheme("settings"); }
    QString id() const override { return "legacy-settings"; }
    bool apply() override;

   private:
    QCheckBox* m_classicWindow = nullptr;
    QComboBox* m_theme = nullptr;
};
