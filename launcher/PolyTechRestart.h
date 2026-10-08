// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PolyTech Launcher - restart requests from the PolyTech Hub mod
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include "BaseInstance.h"

/**
 * Restart requests from the PolyTech Hub mod.
 *
 * The game cannot restart itself: our entry point reads its launch parameters
 * from stdin (see org.prismlauncher.EntryPoint), so a process spawned from inside
 * the game would just wait forever. Instead the mod asks us: right before a clean
 * shutdown it writes <gameRoot>/config/polytechhub/restart.request, and once the
 * game process is gone we launch the instance again.
 *
 * The exit code does not matter: heavily modded Forge often exits with a non-zero
 * code while shutting down (render mods, drivers), long after the mod wrote the
 * request, and the player still pressed "restart". Loop protection instead:
 *  - the mod writes the request only from that button, right before quitting;
 *  - the request file is always deleted when we look at it, acted on or not,
 *    so one request means at most one relaunch;
 *  - a request older than MaxAgeSecs is ignored (e.g. the mod was removed after
 *    writing it, so nothing would ever clean it up);
 *  - the mod itself deletes a leftover request on every start.
 *
 * Header-only on purpose: no CMakeLists changes needed.
 */
namespace PolyTechRestart {

// The mod writes the file right before quitting. Saving the world on a slow
// laptop can take a while, but never this long.
inline constexpr qint64 MaxAgeSecs = 10 * 60;

inline QString requestPath(const BaseInstance* instance)
{
    return QDir(instance->gameRoot()).filePath("config/polytechhub/restart.request");
}

/// Whether there is a fresh restart request. Does not modify anything.
inline bool isRequested(const BaseInstance* instance)
{
    const QFileInfo info(requestPath(instance));
    if (!info.exists()) {
        return false;
    }
    return info.lastModified().secsTo(QDateTime::currentDateTime()) < MaxAgeSecs;
}

/// Takes the request: always deletes the file and tells whether to relaunch.
/// Logs every call, so the launcher log shows whether this code is in the build.
inline bool consume(const BaseInstance* instance, bool gameExitedCleanly)
{
    const QString path = requestPath(instance);
    if (!QFileInfo::exists(path)) {
        qInfo() << "PolyTech: game finished, clean exit:" << gameExitedCleanly << "- no restart request at" << path;
        return false;
    }

    const bool fresh = isRequested(instance);
    if (!QFile::remove(path)) {
        // If the file stays, every following exit would look like a request.
        qWarning() << "PolyTech: could not delete restart request" << path << "- not restarting";
        return false;
    }
    if (!fresh) {
        qInfo() << "PolyTech: restart request ignored, it is older than" << MaxAgeSecs << "seconds";
        return false;
    }
    qInfo() << "PolyTech: restart requested by the mod, clean exit:" << gameExitedCleanly;
    return true;
}

}  // namespace PolyTechRestart
