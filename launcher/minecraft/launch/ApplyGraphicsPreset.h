#pragma once

#include <QImage>
#include <QString>
#include <QVector>

#include "launch/LaunchStep.h"
#include "minecraft/MinecraftInstance.h"

/** One card of the chooser. Texts and icon come from the PolyTech Hub mod jar. */
struct GraphicsPresetChoice {
    QString id;  // potato / creeper / reactor
    QString title;
    QString description;
    QImage icon;  // 16x16 pixel art, may be null
};

/** Static texts of the chooser, also taken from the mod's lang file. */
struct GraphicsPromptTexts {
    QString header;
    QString footnote;
    QString confirm;
    QString selectFirst;
};

/**
 * PolyTech: graphics preset chooser on the very first launch of the pack.
 *
 * The same chooser exists in the PolyTech Hub mod. Doing it here, before the game
 * starts, lets the game come up with the right settings straight away: resource
 * packs, mipmap and shaders are applied at load time instead of by a resource
 * reload in game, which can take minutes or hang a weak PC.
 *
 * Presets are NOT duplicated in the launcher: they are read from the mod jar that
 * packwiz has just downloaded (assets/polytechhub/graphics/<id>.properties), or from
 * config/polytechhub/graphics/<id>.properties, which overrides them — exactly as
 * the mod does. Distant Horizons is left to the mod: the launcher records the choice
 * in config/polytechhub/client.json and the mod applies what is still missing
 * (GraphicsPresetManager.applyStoredIfNeeded) — everything written here already
 * matches, so that costs no resource reload.
 */
class ApplyGraphicsPreset : public LaunchStep {
    Q_OBJECT

   public:
    explicit ApplyGraphicsPreset(LaunchTask* parent, MinecraftInstance* instance);
    ~ApplyGraphicsPreset() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }

   private:
    QString findModJar() const;
    QString findMinecraftJar() const;
    QByteArray readPresetFile(const QString& modJar, const QString& id) const;
    bool applyPreset(const QString& id, const QByteArray& presetData, QString& error);
    bool recordChoice(const QString& id);

    MinecraftInstance* m_instance;
};
