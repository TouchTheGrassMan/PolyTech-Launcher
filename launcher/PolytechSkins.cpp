// SPDX-License-Identifier: GPL-3.0-only
#include "PolytechSkins.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QRegularExpression>

#include "Application.h"

namespace PolytechSkins {

QString accountDir(const QString& nick)
{
    // one folder per nick, whatever the case (Windows paths are case-insensitive anyway)
    QString name = nick.trimmed().toLower();
    name.replace(QRegularExpression(QStringLiteral("[^a-z0-9_\\-]")), QStringLiteral("_"));
    if (name.isEmpty())
        name = QStringLiteral("_");
    return QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("polytechskins/accounts/") + name);
}

QString instanceDir(const QString& gameRoot)
{
    return QDir(gameRoot).filePath(QStringLiteral("polytechskins"));
}

bool read(const QString& dir, QImage& skin, bool& slim)
{
    skin = QImage(QDir(dir).filePath(QStringLiteral("skin.png")));
    slim = false;
    if (skin.isNull())
        return false;
    QFile model(QDir(dir).filePath(QStringLiteral("model.txt")));
    if (model.open(QIODevice::ReadOnly))
        slim = QString::fromUtf8(model.readAll()).trimmed().compare(QStringLiteral("slim"), Qt::CaseInsensitive) == 0;
    return true;
}

bool defaultSkin(QImage& skin)
{
    skin = QImage(QString::fromLatin1(DEFAULT_SKIN_RESOURCE));
    return !skin.isNull();
}

bool effective(const QString& nick, QImage& skin, bool& slim, bool* isDefault)
{
    if (isDefault)
        *isDefault = false;
    if (!nick.isEmpty() && read(accountDir(nick), skin, slim))
        return true;
    slim = false;
    if (isDefault)
        *isDefault = true;
    return defaultSkin(skin);
}

void applyToInstance(const QString& nick, const QString& gameRoot)
{
    if (nick.isEmpty() || gameRoot.isEmpty())
        return;
    const QString toPath = instanceDir(gameRoot);
    QDir().mkpath(toPath);
    const QDir to(toPath);
    const QStringList files = { QStringLiteral("skin.png"), QStringLiteral("model.txt"), QStringLiteral("active.txt") };
    for (const auto& name : files)
        QFile::remove(to.filePath(name));

    const QDir from(accountDir(nick));
    if (QFile::exists(from.filePath(QStringLiteral("skin.png")))) {
        for (const auto& name : files) {
            if (QFile::exists(from.filePath(name)))
                QFile::copy(from.filePath(name), to.filePath(name));
        }
        return;
    }
    // no skin of its own: the default one, as for everyone who did not choose a skin
    QImage steve;
    if (defaultSkin(steve)) {
        steve.save(to.filePath(QStringLiteral("skin.png")), "PNG");
        QFile model(to.filePath(QStringLiteral("model.txt")));
        if (model.open(QIODevice::WriteOnly | QIODevice::Truncate))
            model.write("classic");
    }
}

}  // namespace PolytechSkins
