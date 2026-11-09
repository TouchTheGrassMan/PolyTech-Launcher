// SPDX-License-Identifier: GPL-3.0-only
#include "LegacyPage.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "ui/MainWindow.h"
#include "ui/obsidium/ObsidiumFrame.h"

namespace {
MainWindow* findMainWindow()
{
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (auto* window = qobject_cast<MainWindow*>(widget))
            return window;
    }
    return nullptr;
}
}  // namespace

LegacyPage::LegacyPage(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);

    auto* intro = new QLabel(tr("Здесь временно собраны команды старого главного окна, которым в новом окне ещё не нашлось места. "
                                "Кнопки работают так же, как раньше."),
                             content);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    if (auto* window = findMainWindow()) {
        for (const auto& group : window->legacyActionGroups()) {
            auto* box = new QGroupBox(group.title, content);
            auto* grid = new QGridLayout(box);
            int index = 0;
            for (auto* action : group.actions) {
                if (!action)
                    continue;
                auto* button = new QToolButton(box);
                button->setDefaultAction(action);
                button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                if (action->menu())
                    button->setPopupMode(QToolButton::MenuButtonPopup);
                grid->addWidget(button, index / 2, index % 2);
                index++;
            }
            layout->addWidget(box);
        }
    } else {
        layout->addWidget(new QLabel(tr("Главное окно не открыто."), content));
    }

    auto* windowBox = new QGroupBox(tr("Главное окно"), content);
    auto* windowLayout = new QVBoxLayout(windowBox);
    auto* themeRow = new QHBoxLayout();
    themeRow->addWidget(new QLabel(tr("Оформление:"), windowBox));
    m_theme = new QComboBox(windowBox);
    const QString current = Obsidium::theme(APPLICATION->settings()->get("ObsidiumTheme").toString()).id;
    for (const auto& theme : Obsidium::themes()) {
        m_theme->addItem(theme.name, theme.id);
        if (theme.id == current)
            m_theme->setCurrentIndex(m_theme->count() - 1);
    }
    themeRow->addWidget(m_theme, 1);
    windowLayout->addLayout(themeRow);
    m_classicWindow = new QCheckBox(tr("Использовать старое главное окно (после перезапуска лаунчера)"), windowBox);
    m_classicWindow->setChecked(APPLICATION->settings()->get("ClassicMainWindow").toBool());
    windowLayout->addWidget(m_classicWindow);
    layout->addWidget(windowBox);

    layout->addStretch(1);
    scroll->setWidget(content);
}

bool LegacyPage::apply()
{
    APPLICATION->settings()->set("ClassicMainWindow", m_classicWindow->isChecked());
    const QString theme = m_theme->currentData().toString();
    if (theme != Obsidium::theme(APPLICATION->settings()->get("ObsidiumTheme").toString()).id) {
        APPLICATION->settings()->set("ObsidiumTheme", theme);
        if (auto* window = findMainWindow())
            window->applyObsidiumTheme();
    }
    return true;
}
