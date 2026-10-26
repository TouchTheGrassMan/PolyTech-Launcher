#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

#include "minecraft/launch/ApplyGraphicsPreset.h"

class QPushButton;

/**
 * Launcher copy of the PolyTech Hub graphics chooser (GraphicsPresetPrompt):
 * same panel, cards, colours and proportions, drawn at Minecraft GUI scale 2.
 */
class GraphicsPresetDialog : public QDialog {
    Q_OBJECT

   public:
    GraphicsPresetDialog(const QVector<GraphicsPresetChoice>& choices, const GraphicsPromptTexts& texts, QWidget* parent = nullptr);

    QString selectedId() const { return m_selected; }

   private:
    GraphicsPromptTexts m_texts;
    QString m_selected;
    QPushButton* m_confirm = nullptr;
};
