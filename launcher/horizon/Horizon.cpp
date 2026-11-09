// SPDX-License-Identifier: GPL-3.0-only
#include "Horizon.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSysInfo>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "settings/SettingsObject.h"

namespace Horizon {

void registerSettings(SettingsObject* settings)
{
    if (!settings || settings->contains(SETTING_ENABLED))
        return;
    settings->registerSetting(SETTING_ENABLED, false);
    settings->registerSetting(SETTING_BUILD, "");
    settings->registerSetting(SETTING_TAG, "");
    settings->registerSetting(SETTING_CHANNEL, "release");
    settings->registerSetting(SETTING_AUTO_UPDATE, true);
}

bool isHorizon(BaseInstance* instance)
{
    if (!instance)
        return false;
    auto* settings = instance->settings();
    if (!settings)
        return false;
    registerSettings(settings);
    return settings->get(SETTING_ENABLED).toBool();
}

QString platformAsset()
{
    // The archives Horizon publishes (docs/api/launch-guide.md, "Pick a build").
    const QString arch = QSysInfo::currentCpuArchitecture();
#if defined(Q_OS_WIN)
    return arch == "arm64" ? QStringLiteral("horizon-windows-arm64.zip") : QStringLiteral("horizon-windows-x64.zip");
#elif defined(Q_OS_MACOS)
    return arch == "arm64" ? QStringLiteral("horizon-macos-arm64.zip") : QStringLiteral("horizon-macos-x64.zip");
#elif defined(Q_OS_LINUX)
    return arch == "x86_64" ? QStringLiteral("horizon-linux-x86_64.tar.gz") : QString();
#else
    Q_UNUSED(arch)
    return {};
#endif
}

QString clientProgramName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("horizon-client.exe");
#else
    return QStringLiteral("horizon-client");
#endif
}

QString prepareProgramName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("horizon-prepare.exe");
#else
    return QStringLiteral("horizon-prepare");
#endif
}

QString rootDir()
{
    // The launcher runs from its data folder (libraries/, assets/ are relative to it as well).
    return QDir::current().absoluteFilePath("horizon");
}

QString buildDir(const QString& key)
{
    return FS::PathCombine(rootDir(), "builds", key);
}

bool isBuildInstalled(const QString& key)
{
    if (key.isEmpty())
        return false;
    const QString dir = buildDir(key);
    return QFileInfo::exists(FS::PathCombine(dir, INSTALLED_MARKER)) && QFileInfo::exists(FS::PathCombine(dir, clientProgramName()));
}

QString preparedDir(const QString& minecraftVersion)
{
    return FS::PathCombine(rootDir(), "prepared", minecraftVersion);
}

QString minecraftDir(const QString& minecraftVersion)
{
    return FS::PathCombine(rootDir(), "minecraft", minecraftVersion);
}

QString Release::key() const
{
    static const QRegularExpression s_unsafe("[^A-Za-z0-9._-]");
    QString out = tag;
    if (tag == "nightly")
        out = QStringLiteral("nightly-%1").arg(published.toUTC().toString("yyyyMMdd-HHmmss"));
    out.replace(s_unsafe, "_");
    return out;
}

QString Release::displayName() const
{
    if (channel == "nightly")  // GitHub's title names its date and commit: "Nightly 2026-11-05 · 66f3981"
        return name.startsWith("Nightly") ? name : QStringLiteral("Nightly %1").arg(published.toLocalTime().toString("yyyy-MM-dd"));
    return tag.startsWith('v') ? tag.mid(1) : tag;
}

Release parseRelease(const QJsonObject& object)
{
    Release out;
    if (object.value("draft").toBool())
        return out;
    out.tag = object.value("tag_name").toString();
    out.name = object.value("name").toString();
    out.published = QDateTime::fromString(object.value("published_at").toString(), Qt::ISODate);
    out.channel = out.tag == "nightly" ? QStringLiteral("nightly") : QStringLiteral("release");
    const QString wanted = platformAsset();
    for (const auto& value : object.value("assets").toArray()) {
        const auto asset = value.toObject();
        const QString name = asset.value("name").toString();
        const QString url = asset.value("browser_download_url").toString();
        if (!wanted.isEmpty() && name == wanted) {
            out.assetUrl = url;
            out.assetSize = static_cast<qint64>(asset.value("size").toDouble());
        } else if (name.startsWith("SHA256SUMS") && !name.endsWith(".sig")) {
            out.checksumAssets.append({ name, url });
        }
    }
    return out;
}

QList<Release> parseReleases(const QByteArray& json)
{
    QList<Release> out;
    const auto doc = QJsonDocument::fromJson(json);
    for (const auto& value : doc.array()) {
        const auto release = parseRelease(value.toObject());
        if (!release.isValid())
            continue;
        // Horizon's own tags: vX.Y.Z and the current Nightly. Launcher releases (launcher@X.Y.Z)
        // have no game archive and are already gone; dated Nightlies duplicate `nightly`.
        const bool game = release.tag == "nightly" || (release.tag.size() > 1 && release.tag[0] == 'v' && release.tag[1].isDigit());
        if (!game)
            continue;
        out.append(release);
    }
    std::sort(out.begin(), out.end(), [](const Release& a, const Release& b) { return a.published > b.published; });
    return out;
}

Release newest(const QList<Release>& releases, const QString& channel)
{
    for (const auto& release : releases) {
        if (release.channel == channel)
            return release;
    }
    // No stable release for this platform yet: the Nightly is all there is.
    if (channel != "nightly") {
        for (const auto& release : releases) {
            if (release.channel == "nightly")
                return release;
        }
    }
    return {};
}

QString releasesUrl()
{
    return QStringLiteral("https://api.github.com/repos/%1/releases?per_page=100").arg(REPOSITORY);
}

QString releaseByTagUrl(const QString& tag)
{
    return QStringLiteral("https://api.github.com/repos/%1/releases/tags/%2").arg(REPOSITORY, tag);
}

QString researchLockUrl(const QString& tag)
{
    return QStringLiteral("https://raw.githubusercontent.com/%1/%2/research-lock.json").arg(REPOSITORY, tag);
}

QString launchApiProbeUrl(const QString& tag)
{
    return QStringLiteral("https://raw.githubusercontent.com/%1/%2/docs/api/launch.md").arg(REPOSITORY, tag);
}

Pins parsePins(const QByteArray& json)
{
    Pins out;
    const auto minecraft = QJsonDocument::fromJson(json).object().value("minecraft").toObject();
    out.minecraft = minecraft.value("version").toString();
    out.manifestSha1 = minecraft.value("manifest_sha1").toString();
    out.clientSha1 = minecraft.value("client_sha1").toString();
    out.serverSha1 = minecraft.value("server_sha1").toString();
    out.javaMajor = minecraft.value("java_major").toInt(25);
    return out;
}

Pins readPins(const QString& buildFolder)
{
    QFile file(FS::PathCombine(buildFolder, "research-lock.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return parsePins(file.readAll());
}

QString versionJsonUrl(const Pins& pins)
{
    return QStringLiteral("https://piston-meta.mojang.com/v1/packages/%1/%2.json").arg(pins.manifestSha1, pins.minecraft);
}

QString checksumFor(const QByteArray& sums, const QString& fileName)
{
    // "<hex>  <name>" or "<hex> *<name>" per line, as sha256sum writes it.
    for (const auto& rawLine : sums.split('\n')) {
        const QString line = QString::fromUtf8(rawLine).trimmed();
        const auto space = line.indexOf(' ');
        if (space <= 0)
            continue;
        QString name = line.mid(space).trimmed();
        if (name.startsWith('*'))
            name = name.mid(1);
        if (name == fileName || name.endsWith('/' + fileName))
            return line.left(space).toLower();
    }
    return {};
}

QString fileHash(const QString& path, int algorithm)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(static_cast<QCryptographicHash::Algorithm>(algorithm));
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

}  // namespace Horizon
