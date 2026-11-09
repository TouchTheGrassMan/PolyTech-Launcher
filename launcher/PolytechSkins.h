// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QImage>
#include <QString>

/// Obsidium: the skins of the polytechskins mod, bound to nicks.
///
/// The mod reads the skin of an instance from <gameRoot>/polytechskins/skin.png (+ model.txt).
/// The launcher keeps the chosen skin per nick in <dataRoot>/polytechskins/accounts/<nick>/ and
/// copies it into the instance when the game is started with that nick.
namespace PolytechSkins {

/// <dataRoot>/polytechskins/accounts/<nick>
QString accountDir(const QString& nick);
/// <gameRoot>/polytechskins
QString instanceDir(const QString& gameRoot);

/// skin.png and model.txt of a folder; false when there is no skin.
bool read(const QString& dir, QImage& skin, bool& slim);

/// The default skin (resources/obsidium/default_skin.png): in the skin library from the start
/// and the skin of every nick that has not chosen another one.
inline constexpr const char* DEFAULT_SKIN_RESOURCE = ":/obsidium/default_skin.png";
inline constexpr const char* DEFAULT_SKIN_ID = "default";
bool defaultSkin(QImage& skin);

/// The skin a nick plays with: its own, else the default one. `isDefault` tells which.
bool effective(const QString& nick, QImage& skin, bool& slim, bool* isDefault = nullptr);

/// Puts the nick's skin into the instance: its own, else the default one.
void applyToInstance(const QString& nick, const QString& gameRoot);

}  // namespace PolytechSkins
