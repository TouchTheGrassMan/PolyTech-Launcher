// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PolyTech Launcher - Horizon support
 *
 *  Horizon (https://github.com/horizon-rust/horizon) is an unofficial rewrite of
 *  Minecraft: Java Edition in Rust. It ships as ready programs (horizon-client,
 *  horizon-prepare) and documents a Launch API for other launchers
 *  (docs/api/launch.md, docs/api/launch-guide.md in its repository):
 *
 *   1. a build is a GitHub release of horizon-rust/horizon, one archive per platform,
 *      checked against the release's SHA256SUMS;
 *   2. horizon-prepare turns the player's own Minecraft files (version JSON, client jar,
 *      libraries, assets, the server jar and a Java of the pinned version) into the
 *      resources the game reads;
 *   3. horizon-client starts with --resources/--game-dir/--home and reads the account
 *      as JSON on standard input.
 *
 *  A Horizon instance is an ordinary vanilla instance of the Minecraft version the build
 *  is made for: the launcher downloads that version's client, libraries, assets and Java
 *  as for any instance, then hands them to horizon-prepare and starts horizon-client
 *  instead of Java.
 */

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

class SettingsObject;
class BaseInstance;

namespace Horizon {

/// The GitHub repository the builds come from.
inline constexpr const char* REPOSITORY = "horizon-rust/horizon";

/// Instance settings (instance.cfg).
inline constexpr const char* SETTING_ENABLED = "Horizon";             // bool: this instance plays Horizon
inline constexpr const char* SETTING_BUILD = "HorizonBuild";          // installed build folder (see Release::key)
inline constexpr const char* SETTING_TAG = "HorizonTag";              // its GitHub tag
inline constexpr const char* SETTING_CHANNEL = "HorizonChannel";      // "release" or "nightly"
inline constexpr const char* SETTING_AUTO_UPDATE = "HorizonAutoUpdate";  // bool

/// Registers the settings above in an instance's settings (once).
void registerSettings(SettingsObject* settings);

/// Whether this instance plays Horizon.
bool isHorizon(BaseInstance* instance);

/// The release asset of this system, empty where Horizon has no build.
QString platformAsset();

/// The name of the game program inside a build.
QString clientProgramName();
QString prepareProgramName();

/// <launcher data>/horizon
QString rootDir();
/// <root>/builds/<key>: one unpacked build.
QString buildDir(const QString& key);
/// Whether the build folder is complete (the install finished).
bool isBuildInstalled(const QString& key);
/// The file written once a build is completely unpacked.
inline constexpr const char* INSTALLED_MARKER = ".polytech-installed";
/// <root>/prepared/<minecraft version>: the resources horizon-prepare writes. They depend on
/// the Minecraft version and on the preparer's layout, which horizon-prepare --check compares,
/// so one folder serves every build of that version and is prepared again only when needed.
QString preparedDir(const QString& minecraftVersion);
/// <root>/minecraft/<version>: Mojang's version JSON and the server jar.
QString minecraftDir(const QString& minecraftVersion);

/// One Horizon build on GitHub.
struct Release {
    QString tag;          // GitHub tag: vX.Y.Z or nightly
    QString name;         // GitHub title
    QDateTime published;  // UTC
    QString channel;      // "release" or "nightly"
    QString assetUrl;     // this platform's archive
    qint64 assetSize = 0;
    QList<QPair<QString, QString>> checksumAssets;  // (name, url) of SHA256SUMS*

    bool isValid() const { return !tag.isEmpty() && !assetUrl.isEmpty(); }
    /// The folder name the build is installed as: the tag of a release; for the moving
    /// `nightly` tag, one that names the moment it was published.
    QString key() const;
    /// What to show the player.
    QString displayName() const;
};

/// The builds for this platform in a GitHub `releases` reply, newest first. Dated copies of
/// Nightlies (`nightly-<date>`, deleted after a week) are left out.
QList<Release> parseReleases(const QByteArray& json);
Release parseRelease(const QJsonObject& object);
/// The newest build of a channel ("release" falls back to the newest Nightly when there is no release).
Release newest(const QList<Release>& releases, const QString& channel);

/// api.github.com URLs.
QString releasesUrl();
QString releaseByTagUrl(const QString& tag);
/// research-lock.json of a tag, which pins the Minecraft version the build is made for.
QString researchLockUrl(const QString& tag);
/// A file only builds with the Launch API have (docs/api/launch.md). The launcher starts Horizon
/// through that API, which Nightlies have had since late October 2026 and releases up to
/// v0.2.97 at least don't: a build without it is not offered.
QString launchApiProbeUrl(const QString& tag);

/// What a build's research-lock.json pins.
struct Pins {
    QString minecraft;     // "26.3"
    QString manifestSha1;  // Mojang's version JSON
    QString clientSha1;
    QString serverSha1;
    int javaMajor = 0;
    bool isValid() const { return !minecraft.isEmpty(); }
};
Pins parsePins(const QByteArray& json);
Pins readPins(const QString& buildFolder);

/// Mojang's version JSON of these pins.
QString versionJsonUrl(const Pins& pins);

/// The SHA-256 of `fileName` in a SHA256SUMS file, lower case, or empty.
QString checksumFor(const QByteArray& sums, const QString& fileName);
/// The hash of a file, lower-case hex, or empty when it can't be read.
QString fileHash(const QString& path, int algorithm /* QCryptographicHash::Algorithm */);

}  // namespace Horizon
