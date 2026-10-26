#include "ApplyGraphicsPreset.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

#include "archive/ArchiveReader.h"
#include "minecraft/Library.h"
#include "minecraft/PackProfile.h"
#include "ui/dialogs/GraphicsPresetDialog.h"

namespace {

// Mirrors GraphicsPreset.choices() in the mod: same ids, same order.
const QStringList PRESET_IDS = { QStringLiteral("potato"), QStringLiteral("creeper"), QStringLiteral("reactor") };

// The potato card uses the vanilla poisonous potato, like the mod does.
const QString POTATO_ICON = QStringLiteral("assets/minecraft/textures/item/poisonous_potato.png");

// Must match HubConfig.GRAPHICS_PROMPT_VERSION in the mod.
const int GRAPHICS_PROMPT_VERSION = 1;

// Minecraft 1.20.1: options.txt data version and resource pack format.
const QString OPTIONS_VERSION = QStringLiteral("3465");
const int RESOURCE_PACK_FORMAT = 15;

QByteArray readFromArchive(const QString& archive, const QString& entry)
{
    if (archive.isEmpty())
        return {};
    MMCZip::ArchiveReader reader(archive);
    auto file = reader.goToFile(entry);
    return file ? file->readAll() : QByteArray();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList splitLines(const QByteArray& data)
{
    QStringList lines = QString::fromUtf8(data).split('\n');
    for (QString& line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
    }
    return lines;
}

bool writeText(const QString& path, const QString& text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(text.toUtf8());
    return true;
}

QJsonObject readJsonObject(const QByteArray& data)
{
    const auto doc = QJsonDocument::fromJson(data);
    return doc.isObject() ? doc.object() : QJsonObject();
}

/** The subset of java.util.Properties the presets use: key=value / key:value, # and ! comments. */
QMap<QString, QString> parseProperties(const QByteArray& data)
{
    QMap<QString, QString> props;
    for (const QString& raw : splitLines(data)) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('!'))
            continue;
        int sep = -1;
        for (int i = 0; i < line.size(); ++i) {
            if (line[i] == '=' || line[i] == ':') {
                sep = i;
                break;
            }
        }
        if (sep <= 0)
            continue;
        props.insert(line.left(sep).trimmed(), line.mid(sep + 1).trimmed());
    }
    return props;
}

QStringList jsonStringArray(const QString& value)
{
    QStringList out;
    for (const auto& item : QJsonDocument::fromJson(value.toUtf8()).array())
        out.append(item.toString());
    return out;
}

QString toJsonArray(const QStringList& list)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(list)).toJson(QJsonDocument::Compact));
}

/** Like Java's Double.toString for the values we use: 0.75, 1.0, 0.0. */
QString formatDouble(double value)
{
    QString text = QString::number(value, 'g', 15);
    if (!text.contains('.') && !text.contains('e'))
        text += QStringLiteral(".0");
    return text;
}

}  // namespace

ApplyGraphicsPreset::ApplyGraphicsPreset(LaunchTask* parent, MinecraftInstance* instance) : LaunchStep(parent), m_instance(instance) {}

QString ApplyGraphicsPreset::findModJar() const
{
    const QDir mods(QDir(m_instance->gameRoot()).filePath(QStringLiteral("mods")));
    for (const QFileInfo& jar : mods.entryInfoList({ QStringLiteral("*.jar") }, QDir::Files, QDir::Name)) {
        if (!jar.fileName().contains(QStringLiteral("polytechhub"), Qt::CaseInsensitive))
            continue;
        if (!readFromArchive(jar.absoluteFilePath(), QStringLiteral("assets/polytechhub/graphics/potato.properties")).isEmpty())
            return jar.absoluteFilePath();
    }
    return {};
}

QString ApplyGraphicsPreset::findMinecraftJar() const
{
    auto components = m_instance->getPackProfile();
    if (!components)
        return {};
    auto profile = components->getProfile();
    if (!profile)
        return {};
    auto mainJar = profile->getMainJar();
    if (!mainJar)
        return {};
    QStringList jars, native, native32, native64;
    mainJar->getApplicableFiles(m_instance->runtimeContext(), jars, native, native32, native64, m_instance->getLocalLibraryPath());
    for (const QString& jar : jars) {
        if (QFile::exists(jar))
            return jar;
    }
    return {};
}

/** Same lookup as GraphicsSettings.load in the mod: config override first, then the jar. */
QByteArray ApplyGraphicsPreset::readPresetFile(const QString& modJar, const QString& id) const
{
    const QString override =
        QDir(m_instance->gameRoot()).filePath(QStringLiteral("config/polytechhub/graphics/%1.properties").arg(id));
    if (QFile::exists(override)) {
        const QByteArray data = readFile(override);
        if (!data.isEmpty())
            return data;
    }
    return readFromArchive(modJar, QStringLiteral("assets/polytechhub/graphics/%1.properties").arg(id));
}

void ApplyGraphicsPreset::executeTask()
{
    const QDir game(m_instance->gameRoot());

    // No PolyTech Hub in the pack (or the pack failed to download) — nothing to offer.
    const QString modJar = findModJar();
    if (modJar.isEmpty()) {
        emitSucceeded();
        return;
    }

    // The game has run before: settings belong to the player, keep them.
    if (QFile::exists(game.filePath(QStringLiteral("options.txt")))) {
        emitSucceeded();
        return;
    }

    // Already chosen (e.g. in game) — the mod's own rule from HubConfig.needsGraphicsPrompt.
    const QString clientJson = game.filePath(QStringLiteral("config/polytechhub/client.json"));
    {
        const QJsonObject client = readJsonObject(readFile(clientJson));
        const QString chosen = client.value(QStringLiteral("graphicsPreset")).toString(QStringLiteral("unset"));
        if (chosen != QLatin1String("unset") && client.value(QStringLiteral("graphicsPromptVersion")).toInt() >= GRAPHICS_PROMPT_VERSION) {
            emitSucceeded();
            return;
        }
    }

    // Texts come from the mod's own lang file so both choosers read the same.
    QJsonObject lang = readJsonObject(readFromArchive(modJar, QStringLiteral("assets/polytechhub/lang/ru_ru.json")));
    if (lang.isEmpty())
        lang = readJsonObject(readFromArchive(modJar, QStringLiteral("assets/polytechhub/lang/en_us.json")));
    auto text = [&lang](const QString& key, const QString& fallback) { return lang.value(key).toString(fallback); };

    GraphicsPromptTexts texts;
    texts.header = text(QStringLiteral("polytechhub.graphics.header"),
                        QStringLiteral("Давайте настроим качество изображения, как бы вы оценили свой компьютер?"));
    texts.footnote = text(QStringLiteral("polytechhub.graphics.footnote"),
                          QStringLiteral("Вы сможете выбрать другой набор настроек графики позже в настройках, если вас не устроит результат"));
    texts.confirm = text(QStringLiteral("polytechhub.graphics.confirm"), QStringLiteral("Подтвердить выбор"));
    texts.selectFirst = text(QStringLiteral("polytechhub.graphics.select_first"), QStringLiteral("Выберите вариант"));

    const QString minecraftJar = findMinecraftJar();
    QVector<GraphicsPresetChoice> choices;
    for (const QString& id : PRESET_IDS) {
        GraphicsPresetChoice choice;
        choice.id = id;
        choice.title = text(QStringLiteral("polytechhub.graphics.%1.title").arg(id), id);
        choice.description = text(QStringLiteral("polytechhub.graphics.%1.desc").arg(id), QString());
        const QByteArray icon = id == QLatin1String("potato")
                                    ? readFromArchive(minecraftJar, POTATO_ICON)
                                    : readFromArchive(modJar, QStringLiteral("assets/polytechhub/textures/gui/preset_%1.png").arg(id));
        choice.icon = QImage::fromData(icon);
        choices.append(choice);
    }

    GraphicsPresetDialog dialog(choices, texts);
    if (dialog.exec() != QDialog::Accepted) {
        // Nothing recorded: the mod offers its own chooser in game.
        emit logLine(tr("Graphics preset was not chosen, it will be offered in game."), MessageLevel::Launcher);
        emitSucceeded();
        return;
    }

    const QString id = dialog.selectedId();
    QString title = id;
    for (const auto& choice : choices) {
        if (choice.id == id)
            title = choice.title;
    }

    QString error;
    const QByteArray preset = readPresetFile(modJar, id);
    if (preset.isEmpty())
        error = tr("preset file not found");
    if (error.isEmpty() && applyPreset(id, preset, error) && recordChoice(id)) {
        emit logLine(tr("Graphics preset applied: %1").arg(title), MessageLevel::Launcher);
    } else {
        // Never block the launch over this; the mod will ask again in game.
        emit logLine(tr("Could not apply graphics preset %1: %2").arg(title, error.isEmpty() ? tr("write failed") : error),
                     MessageLevel::Warning);
    }
    emitSucceeded();
}

/**
 * Writes the preset to disk the way VanillaGraphicsApplier + OculusBridge apply it in
 * game: options.txt in the game's own value format, resource packs enabled/disabled
 * one by one (not a full list), shaders on/off in oculus.properties. Distant Horizons
 * is applied by the mod at startup.
 */
bool ApplyGraphicsPreset::applyPreset(const QString& id, const QByteArray& presetData, QString& error)
{
    const auto preset = parseProperties(presetData);
    if (preset.isEmpty()) {
        error = tr("preset %1 is empty").arg(id);
        return false;
    }
    auto get = [&preset](const QString& key) { return preset.value(key).trimmed(); };
    auto warn = [this](const QString& message) { emit logLine(tr("Graphics preset: %1").arg(message), MessageLevel::Warning); };

    const QDir game(m_instance->gameRoot());
    const QString optionsPath = game.filePath(QStringLiteral("options.txt"));

    // Current options.txt (absent on a first launch, merged anyway), order preserved.
    QStringList order;
    QMap<QString, QString> options;
    for (const QString& line : splitLines(readFile(optionsPath))) {
        const int sep = line.indexOf(':');
        if (sep <= 0)
            continue;
        const QString key = line.left(sep);
        if (!options.contains(key))
            order.append(key);
        options.insert(key, line.mid(sep + 1));
    }
    auto set = [&](const QString& key, const QString& value) {
        if (!options.contains(key))
            order.append(key);
        options.insert(key, value);
    };

    // --- plain values ---
    for (const QString& key : { QStringLiteral("renderDistance"), QStringLiteral("simulationDistance"), QStringLiteral("biomeBlendRadius"),
                                QStringLiteral("maxFps"), QStringLiteral("mipmapLevels") }) {
        const QString raw = get(key);
        if (raw.isEmpty())
            continue;
        bool ok = false;
        const int value = raw.toInt(&ok);
        if (ok)
            set(key, QString::number(value));
        else
            warn(tr("%1 = %2 — expected an integer").arg(key, raw));
    }
    for (const QString& key : { QStringLiteral("ao"), QStringLiteral("entityShadows"), QStringLiteral("enableVsync"), QStringLiteral("bobView") }) {
        const QString raw = get(key).toLower();
        if (raw.isEmpty())
            continue;
        if (raw == QLatin1String("true") || raw == QLatin1String("false"))
            set(key, raw);
        else
            warn(tr("%1 = %2 — expected true or false").arg(key, raw));
    }
    for (const QString& key : { QStringLiteral("entityDistanceScaling"), QStringLiteral("screenEffectScale") }) {
        const QString raw = get(key);
        if (raw.isEmpty())
            continue;
        bool ok = false;
        const double value = raw.toDouble(&ok);
        if (ok)
            set(key, formatDouble(value));
        else
            warn(tr("%1 = %2 — expected a number").arg(key, raw));
    }

    // --- enums, in the game's own storage format ---
    auto setEnum = [&](const QString& key, const QMap<QString, QString>& values) {
        const QString raw = get(key).toLower();
        if (raw.isEmpty())
            return;
        if (values.contains(raw))
            set(key, values.value(raw));
        else
            warn(tr("%1 = %2 — unknown value").arg(key, raw));
    };
    setEnum(QStringLiteral("graphicsMode"), { { "fast", "0" }, { "0", "0" }, { "fancy", "1" }, { "1", "1" }, { "fabulous", "2" }, { "2", "2" } });
    setEnum(QStringLiteral("particles"),
            { { "all", "0" }, { "0", "0" }, { "decreased", "1" }, { "1", "1" }, { "minimal", "2" }, { "2", "2" } });
    setEnum(QStringLiteral("renderClouds"),
            { { "off", "\"false\"" }, { "false", "\"false\"" }, { "fast", "\"fast\"" }, { "fancy", "\"true\"" }, { "true", "\"true\"" } });
    setEnum(QStringLiteral("prioritizeChunkUpdates"), { { "none", "0" },
                                                         { "0", "0" },
                                                         { "player_affected", "1" },
                                                         { "1", "1" },
                                                         { "nearby", "2" },
                                                         { "2", "2" } });

    // --- resource packs: switch the named ones, leave the rest alone ---
    auto list = [&get](const QString& key) {
        QStringList out;
        for (const QString& part : get(key).split(',')) {
            if (!part.trimmed().isEmpty())
                out.append(part.trimmed());
        }
        return out;
    };
    const QStringList disable = list(QStringLiteral("resourcePacks.disable"));
    const QStringList enable = list(QStringLiteral("resourcePacks.enable"));
    if (!disable.isEmpty() || !enable.isEmpty()) {
        QStringList selected = jsonStringArray(options.value(QStringLiteral("resourcePacks")));
        const QStringList incompatibleBefore = jsonStringArray(options.value(QStringLiteral("incompatibleResourcePacks")));

        auto packPath = [&game](const QString& packId) {
            return packId.startsWith(QLatin1String("file/")) ? game.filePath(QStringLiteral("resourcepacks/") + packId.mid(5)) : QString();
        };

        for (const QString& packId : disable)
            selected.removeAll(packId);
        for (const QString& packId : enable) {
            if (selected.contains(packId))
                continue;
            const QString path = packPath(packId);
            if (!path.isEmpty() && !QFileInfo::exists(path)) {
                warn(tr("resource pack %1 not found — skipped").arg(packId));
                continue;
            }
            selected.append(packId);  // last = highest priority
        }

        // Like the mod: packs made for another version go to the incompatible list too,
        // or the game drops them on load. If unsure, list it — that is harmless.
        QStringList incompatible;
        for (const QString& packId : selected) {
            const QString path = packPath(packId);
            if (path.isEmpty()) {
                if (incompatibleBefore.contains(packId))
                    incompatible.append(packId);
                continue;
            }
            const QByteArray meta = QFileInfo(path).isDir() ? readFile(QDir(path).filePath(QStringLiteral("pack.mcmeta")))
                                                            : readFromArchive(path, QStringLiteral("pack.mcmeta"));
            const int format = readJsonObject(meta).value(QStringLiteral("pack")).toObject().value(QStringLiteral("pack_format")).toInt(-1);
            if (format != RESOURCE_PACK_FORMAT)
                incompatible.append(packId);
        }

        set(QStringLiteral("resourcePacks"), toJsonArray(selected));
        set(QStringLiteral("incompatibleResourcePacks"), toJsonArray(incompatible));
    }

    // Without a version the game treats options.txt as ancient and runs data fixers on it.
    if (!options.contains(QStringLiteral("version"))) {
        order.prepend(QStringLiteral("version"));
        options.insert(QStringLiteral("version"), OPTIONS_VERSION);
    }

    QStringList lines;
    for (const QString& key : order)
        lines.append(key + ':' + options.value(key));
    if (!writeText(optionsPath, lines.join('\n') + '\n')) {
        error = tr("cannot write options.txt");
        return false;
    }

    // --- shaders: Oculus reads enableShaders at startup; the shader pack name stays as is ---
    const QString shaders = get(QStringLiteral("oculus.enableShaders")).toLower();
    if (shaders == QLatin1String("true") || shaders == QLatin1String("false")) {
        bool oculus = false;
        const QDir mods(game.filePath(QStringLiteral("mods")));
        for (const QString& jar : mods.entryList({ QStringLiteral("*.jar") }, QDir::Files)) {
            if (jar.contains(QStringLiteral("oculus"), Qt::CaseInsensitive) || jar.contains(QStringLiteral("iris"), Qt::CaseInsensitive))
                oculus = true;
        }
        if (oculus) {
            const QString path = game.filePath(QStringLiteral("config/oculus.properties"));
            QStringList out;
            bool replaced = false;
            for (const QString& line : splitLines(readFile(path))) {
                const QString trimmed = line.trimmed();
                if (trimmed.startsWith(QLatin1String("enableShaders")) &&
                    (trimmed.size() == 13 || trimmed[13] == '=' || trimmed[13] == ':' || trimmed[13].isSpace())) {
                    out.append(QStringLiteral("enableShaders=") + shaders);
                    replaced = true;
                } else if (!line.isEmpty()) {
                    out.append(line);
                }
            }
            if (!replaced)
                out.append(QStringLiteral("enableShaders=") + shaders);
            if (!writeText(path, out.join('\n') + '\n')) {
                error = tr("cannot write oculus.properties");
                return false;
            }
        }
    } else if (!shaders.isEmpty()) {
        warn(tr("oculus.enableShaders = %1 — expected true or false").arg(shaders));
    }
    return true;
}

/**
 * Records the choice in the mod's config. graphicsApplied is left alone on purpose:
 * the mod sees "chosen but not applied" and applies the preset itself at startup,
 * which sets up Distant Horizons. Everything written above already matches, so for
 * the rest that is a no-op — no resource reload, no shader recompile.
 */
bool ApplyGraphicsPreset::recordChoice(const QString& id)
{
    const QString path = QDir(m_instance->gameRoot()).filePath(QStringLiteral("config/polytechhub/client.json"));
    QJsonObject client = readJsonObject(readFile(path));
    client.insert(QStringLiteral("graphicsPreset"), id);
    client.insert(QStringLiteral("graphicsPromptVersion"), GRAPHICS_PROMPT_VERSION);
    return writeText(path, QString::fromUtf8(QJsonDocument(client).toJson(QJsonDocument::Indented)));
}
