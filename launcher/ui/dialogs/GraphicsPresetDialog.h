#pragma once

#include <QString>
#include <QVector>

#include "minecraft/HardwareRating.h"
#include "minecraft/launch/ApplyGraphicsPreset.h"
#include "ui/obsidium/ObsidiumDialog.h"

class QPushButton;

/**
 * The graphics preset chooser of the first launch, in the launcher's look: the cards of the
 * PolyTech Hub chooser (GraphicsPresetPrompt: same icons and texts), what this computer is and
 * which preset suits it, and a warning when it is below the pack's minimum. The suggested preset
 * is chosen from the start, so pressing the button right away launches with it.
 */
class GraphicsPresetDialog : public ObsidiumDialog {
    Q_OBJECT

   public:
    GraphicsPresetDialog(const QVector<GraphicsPresetChoice>& choices,
                         const GraphicsPromptTexts& texts,
                         const HardwareRating::Assessment& hardware,
                         QWidget* parent = nullptr);

    QString selectedId() const { return m_selected; }

   private:
    GraphicsPromptTexts m_texts;
    QString m_selected;
    QPushButton* m_confirm = nullptr;
};
