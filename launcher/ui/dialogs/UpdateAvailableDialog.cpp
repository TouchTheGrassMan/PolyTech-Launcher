// SPDX-FileCopyrightText: 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "UpdateAvailableDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "BuildConfig.h"
#include "Markdown.h"
#include "StringUtils.h"
#include "ui/obsidium/ObsidiumFrame.h"

UpdateAvailableDialog::UpdateAvailableDialog(const QString& currentVersion,
                                             const QString& availableVersion,
                                             const QString& releaseNotes,
                                             QWidget* parent)
    : ObsidiumDialog(parent)
{
    using namespace ObsidiumStyle;
    setTitle(tr("Доступно обновление"));
    setFixedSize(520, 400);

    auto* headline = new QLabel(tr("Вышла новая версия %1 — %2").arg(BuildConfig.LAUNCHER_DISPLAYNAME, availableVersion), this);
    headline->setWordWrap(true);
    headline->setStyleSheet(hintCss());
    body()->addWidget(headline);

    auto* current = new QLabel(tr("Сейчас у вас версия %1. Что нового:").arg(currentVersion), this);
    current->setStyleSheet(labelCss(MUTED, SMALL_PX));
    body()->addWidget(current);

    auto* notes = new QTextBrowser(this);
    notes->setObjectName(QStringLiteral("releaseNotes"));
    notes->setOpenExternalLinks(true);
    notes->setHtml(StringUtils::htmlListPatch(markdownToHTML(releaseNotes)));
    notes->setStyleSheet(progressCss() + QStringLiteral("QTextBrowser { %1 color: %2; background: %3; border: 2px solid %4; border-radius: 6px; "
                                                        "padding: 4px; }")
                                             .arg(fontCss(TEXT_PX), rgb(Obsidium::TEXT_COLOR), rgb(PANEL_BG), rgb(BRONZE)));
    body()->addWidget(notes, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* laterButton = new QPushButton(tr("Позже"), this);
    laterButton->setStyleSheet(buttonCss());
    laterButton->setCursor(Qt::PointingHandCursor);
    laterButton->setAutoDefault(false);
    auto* installButton = new QPushButton(tr("Обновить"), this);
    installButton->setStyleSheet(accentButtonCss());
    installButton->setCursor(Qt::PointingHandCursor);
    installButton->setDefault(true);
    buttons->addWidget(laterButton);
    buttons->addWidget(installButton);
    body()->addLayout(buttons);

    connect(laterButton, &QPushButton::clicked, this, [this] { done(ResultCode::DontInstall); });
    connect(installButton, &QPushButton::clicked, this, [this] { done(ResultCode::Install); });
    installButton->setFocus();
}
