// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Authors: Andrew Okin
 *               Peterix
 *               Orochimarufan <orochimarufan.x3@gmail.com>
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once

#include <memory>

#include <QMainWindow>
#include <QProcess>
#include <QTimer>

#include "BaseInstance.h"
#include "minecraft/auth/MinecraftAccount.h"

class LaunchController;
class NewsChecker;
class QToolButton;
class InstanceProxyModel;
class LabeledToolButton;
class QLabel;
class MinecraftLauncher;
class BaseProfilerFactory;
class InstanceView;
class ObsidiumFrame;
class DeveloperPage;
class ObsidiumButton;
class QStackedWidget;
class QTextBrowser;
class QPushButton;
class ObsidiumInstanceList;
class SkinPreview3D;
class KonamiCode;
class InstanceTask;
class LabeledToolButton;

namespace Ui {
class MainWindow;
}
class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    /// Obsidium: the old window's commands that the drawn one doesn't show yet, for the
    /// "Старые настройки" page of the settings.
    struct LegacyGroup {
        QString title;
        QList<QAction*> actions;
    };
    QList<LegacyGroup> legacyActionGroups();
    /// Obsidium: shows the theme chosen in the settings ("ObsidiumTheme").
    void applyObsidiumTheme();

    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

    bool eventFilter(QObject* obj, QEvent* ev) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

    void checkInstancePathForProblems();

    void updatesAllowedChanged(bool allowed);

    void processURLs(QList<QUrl> urls);
   signals:
    void isClosing();

   protected:
    QMenu* createPopupMenu() override;

   private slots:
    void on_actionAbout_triggered();

    void on_actionAddInstance_triggered();

    void on_actionREDDIT_triggered();

    void on_actionMATRIX_triggered();

    void on_actionDISCORD_triggered();

    void on_actionCopyInstance_triggered();

    void on_actionChangeInstGroup_triggered();

    void on_actionChangeInstIcon_triggered();

    void on_actionViewLauncherRootFolder_triggered();

    void on_actionViewInstanceFolder_triggered();
    void on_actionViewCentralModsFolder_triggered();

    void on_actionViewIconThemeFolder_triggered();
    void on_actionViewWidgetThemeFolder_triggered();
    void on_actionViewIconsFolder_triggered();
    void on_actionViewLogsFolder_triggered();
    void on_actionViewJavaFolder_triggered();

    void on_actionViewSkinsFolder_triggered();

    void on_actionViewSelectedInstFolder_triggered();

    void refreshInstances();

    void checkForUpdates();

    void on_actionSettings_triggered();

    void on_actionManageSkins_triggered();

    void on_actionManageAccounts_triggered();

    void on_actionReportBug_triggered();

    void on_actionClearMetadata_triggered();

#ifdef Q_OS_MAC
    void on_actionAddToPATH_triggered();
#endif

    void on_actionOpenWiki_triggered();

    void on_actionMoreNews_triggered();

    void newsButtonClicked();

    void on_actionLaunchInstance_triggered();

    void on_actionKillInstance_triggered();

    void on_actionDeleteInstance_triggered();

    void deleteGroup(QString group);
    void renameGroup(QString group);
    void undoTrashInstance();

    inline void on_actionExportInstance_triggered() { on_actionExportInstanceZip_triggered(); }
    void on_actionExportInstanceZip_triggered();
    void on_actionExportInstanceMrPack_triggered();
    void on_actionExportInstanceFlamePack_triggered();

    void on_actionRenameInstance_triggered();

    void on_actionEditInstance_triggered();

    void on_actionCreateInstanceShortcut_triggered();
    void on_actionChangeSkin_triggered();

    void taskEnd();

    /**
     * called when an icon is changed in the icon model.
     */
    void iconUpdated(QString);

    void showInstanceContextMenu(const QPoint&);

    void updateMainToolBar();

    void updateLaunchButton();

    void updateThemeMenu();

    void instanceActivated(QModelIndex);

    void instanceChanged(const QModelIndex& current, const QModelIndex& previous);

    void instanceSelectRequest(QString id);

    void instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

    void selectionBad();

    void startTask(Task* task);

    void defaultAccountChanged();

    void changeActiveAccount();

    void repopulateAccountsMenu();

    void updateNewsLabel();

    void konamiTriggered();

    void globalSettingsClosed();

    void setStatusBarVisibility(bool);

    void lockToolbars(bool);

#ifndef Q_OS_MAC
    void keyReleaseEvent(QKeyEvent* event) override;
#endif

    void refreshCurrentInstance();

   protected:
    void showEvent(QShowEvent* event) override;

   private:
    void retranslateUi();

    // Obsidium: the drawn main window (ui/obsidium). The classic toolbars, menus and the
    // status bar still exist and own the actions; they are only hidden.
    void setupObsidiumWindow();
    void hideClassicChrome();
    void showObsidiumPage(int page);
    void resizeObsidiumWindow();
    void selectFirstVisibleInstance();
    void updateObsidiumAccount();
    void updateObsidiumSkin();
    void updateObsidiumNews();
    void updateObsidiumFonts(qreal scale);

    void addInstance(const QString& url = QString(), const QMap<QString, QString>& extra_info = {});
    void activateInstance(BaseInstance* instance);
    void updateInstanceToolIcon(QString new_icon);
    void setSelectedInstanceById(const QString& id);
    void updateStatusCenter();
    void setInstanceActionsEnabled(bool enabled);

    void runModalTask(Task* task);
    void instanceFromInstanceTask(InstanceTask* task);

   private:
    Ui::MainWindow* ui;
    // these are managed by Qt's memory management model!
    InstanceView* view = nullptr;
    InstanceProxyModel* proxymodel = nullptr;
    QToolButton* newsLabel = nullptr;
    QLabel* m_statusLeft = nullptr;
    QLabel* m_statusCenter = nullptr;
    LabeledToolButton* changeIconButton = nullptr;
    LabeledToolButton* renameButton = nullptr;
    QToolButton* helpMenuButton = nullptr;
    KonamiCode* secretEventFilter = nullptr;

    std::shared_ptr<Setting> instanceToolbarSetting = nullptr;

    unique_qobject_ptr<NewsChecker> m_newsChecker;

    BaseInstance* m_selectedInstance = nullptr;
    QString m_currentInstIcon;

    // managed by the application object
    Task* m_versionLoadTask = nullptr;

    // Obsidium window (null when the classic window is used)
    ObsidiumFrame* m_obsidium = nullptr;
    QList<ObsidiumButton*> m_obsTabs;
    QStackedWidget* m_obsPages = nullptr;
    ObsidiumInstanceList* m_obsList = nullptr;
    QMenu* m_obsActionsMenu = nullptr;
    QString m_obsLastUserInstance;  // the user's instance chosen when the tab was left
    QTextBrowser* m_obsNews = nullptr;
    QLabel* m_obsNick = nullptr;
    SkinPreview3D* m_obsSkin = nullptr;
    bool m_obsSized = false;
    ObsidiumButton* m_obsPlay = nullptr;
    DeveloperPage* m_obsDeveloper = nullptr;
    ObsidiumButton* m_obsProfile = nullptr;
    QAction* m_legacyRename = nullptr;
    QAction* m_legacyIcon = nullptr;
};
