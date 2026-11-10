// SPDX-FileCopyrightText: 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "ui/obsidium/ObsidiumDialog.h"

/// Obsidium: "a new version is out" in the launcher's look - "Обновить" or "Позже".
class UpdateAvailableDialog : public ObsidiumDialog {
    Q_OBJECT

   public:
    enum ResultCode {
        Install = 10,
        DontInstall = 11,
        Skip = 12,  // (no longer offered)
    };

    explicit UpdateAvailableDialog(const QString& currentVersion,
                                   const QString& availableVersion,
                                   const QString& releaseNotes,
                                   QWidget* parent = 0);
    ~UpdateAvailableDialog() = default;
};
