// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Obsidium - the drawn main window.
 *
 *  MainWindow keeps all its actions, menus and slots; this file only builds a different face
 *  for them: the picture of ui/obsidium/ObsidiumFrame with buttons that trigger the same actions.
 *  Where the buttons and panels are is up to the theme (ui/obsidium/ObsidiumFrame.cpp).
 */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QCursor>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "PolytechSkins.h"
#include "minecraft/auth/AccountList.h"
#include "news/NewsChecker.h"
#include "ui/dialogs/SkinPreview3D.h"
#include "InstanceList.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/obsidium/DeveloperPage.h"
#include "ui/obsidium/ObsidiumDialog.h"
#include "ui/obsidium/ObsidiumFrame.h"

namespace {

const char* PANEL_STYLE =
    "QAbstractScrollArea { background: transparent; border: none; }"
    "QTextBrowser { color: #e9ddc9; }"
    "QScrollBar:vertical { background: rgba(0,0,0,60); width: 10px; margin: 0; }"
    "QScrollBar::handle:vertical { background: #6b3f8f; min-height: 24px; border-radius: 4px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }"
    "QScrollBar::add-page, QScrollBar::sub-page { background: none; }";

const char* ACTIONS_STYLE =
    "QPushButton { color: #e9ddc9; background: rgba(60, 30, 80, 200); border: 1px solid #8a4fb8; border-radius: 4px; padding: 3px 12px; }"
    "QPushButton:hover { background: rgba(110, 50, 150, 220); }"
    "QPushButton::menu-indicator { subcontrol-position: right center; right: 4px; }";

}  // namespace

void MainWindow::setupObsidiumWindow()
{
    using Style = ObsidiumButton::Style;
    using Obsidium::Slot;
    const Slot TAB_SLOTS[3] = { Slot::Tab1, Slot::Tab2, Slot::Tab3 };

    m_obsidium = new ObsidiumFrame(this);
    m_obsidium->setTheme(Obsidium::theme(APPLICATION->settings()->get("ObsidiumTheme").toString()));
    m_obsidium->dimForDialogs(this);

    // The picture is the window: no system frame, and its transparent edges stay transparent.
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);

    // The instance list moves into the picture; the old central widget is replaced.
    ui->horizontalLayout->removeWidget(view);
    view->setParent(nullptr);
    QWidget* oldCentral = takeCentralWidget();
    setCentralWidget(m_obsidium);
    // Other code (the Konami easter egg) still reaches for these.
    ui->centralWidget = m_obsidium;
    ui->horizontalLayout = nullptr;
    if (oldCentral)
        oldCentral->deleteLater();
    hideClassicChrome();

    // Hidden toolbars and menus don't fire shortcuts: give them to the window itself.
    addActions(ui->fileMenu->actions());
    addAction(ui->actionUndoTrashInstance);

    // ---- window buttons
    auto* minimizeButton = m_obsidium->addButton({}, Style::Bare, Slot::Minimize, 0);
    minimizeButton->setToolTip(tr("Свернуть"));
    connect(minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    auto* closeButton = m_obsidium->addButton({}, Style::Bare, Slot::Close, 0);
    closeButton->setToolTip(tr("Закрыть"));
    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    // ---- left column
    auto* addButton = m_obsidium->addButton(tr("Добавить сборку"), Style::Plain, Slot::Left1, 21);
    connect(addButton, &QAbstractButton::clicked, ui->actionAddInstance, &QAction::trigger);

    auto* settingsButton = m_obsidium->addButton(tr("Настройки"), Style::Plain, Slot::Left2, 21);
    connect(settingsButton, &QAbstractButton::clicked, ui->actionSettings, &QAction::trigger);

    auto* filesButton = m_obsidium->addButton(tr("Файлы"), Style::Plain, Slot::Left3, 21);
    connect(filesButton, &QAbstractButton::clicked, this, [this, filesButton] { ui->foldersMenu->popup(filesButton->mapToGlobal(QPoint(0, filesButton->height()))); });

    auto* siteButton = m_obsidium->addButton(tr("Открыть сайт"), Style::Plain, Slot::Left4, 21);
    siteButton->setToolTip(tr("Скоро"));

    auto* updateButton = m_obsidium->addButton(tr("Обновление"), Style::Plain, Slot::Left5, 21);
    connect(updateButton, &QAbstractButton::clicked, ui->actionCheckUpdate, &QAction::trigger);
    auto syncUpdate = [this, updateButton] { updateButton->setEnabled(ui->actionCheckUpdate->isVisible() && ui->actionCheckUpdate->isEnabled()); };
    connect(ui->actionCheckUpdate, &QAction::changed, updateButton, syncUpdate);
    syncUpdate();

    // ---- tabs
    const QString tabNames[3] = { tr("От разработчика"), tr("Пользовательские"), tr("Новости") };
    for (int i = 0; i < 3; i++) {
        auto* tabButton = m_obsidium->addButton(tabNames[i], Style::Tab, TAB_SLOTS[i], 12);
        connect(tabButton, &QAbstractButton::clicked, this, [this, i] { showObsidiumPage(i); });
        m_obsTabs.append(tabButton);
    }

    // ---- central panel: the instances (pages 1 and 2 filter the same list) and the news
    m_obsPages = new QStackedWidget();
    m_obsPages->setStyleSheet(PANEL_STYLE);

    auto* instancesPage = new QWidget();
    auto* instancesLayout = new QVBoxLayout(instancesPage);
    instancesLayout->setContentsMargins(6, 4, 6, 4);
    instancesLayout->setSpacing(4);
    auto* topBar = new QHBoxLayout();
    m_obsInstanceName = new QLabel();
    m_obsInstanceName->setStyleSheet("color: #e9ddc9;");
    topBar->addWidget(m_obsInstanceName, 1);
    // Temporary: the selected instance's actions, until they get their own place in the picture.
    m_obsActions = new QPushButton(tr("Действия"));
    m_obsActions->setStyleSheet(ACTIONS_STYLE);
    m_obsActions->setCursor(Qt::PointingHandCursor);
    m_obsActions->setFocusPolicy(Qt::NoFocus);
    auto* actionsMenu = new QMenu(m_obsActions);
    actionsMenu->addAction(ui->actionEditInstance);
    actionsMenu->addAction(ui->actionChangeInstGroup);
    actionsMenu->addAction(ui->actionViewSelectedInstFolder);
    actionsMenu->addAction(ui->actionExportInstance);
    actionsMenu->addAction(ui->actionCopyInstance);
    actionsMenu->addAction(ui->actionDeleteInstance);
    actionsMenu->addAction(ui->actionCreateInstanceShortcut);
    actionsMenu->addAction(ui->actionChangeSkin);
    m_obsActions->setMenu(actionsMenu);
    topBar->addWidget(m_obsActions);
    instancesLayout->addLayout(topBar);

    view->setParent(instancesPage);
    view->setAutoFillBackground(false);
    view->viewport()->setAutoFillBackground(false);
    {
        QPalette pal = view->palette();
        pal.setColor(QPalette::Base, Qt::transparent);
        pal.setColor(QPalette::Window, Qt::transparent);
        pal.setColor(QPalette::Text, QColor(233, 221, 201));
        pal.setColor(QPalette::WindowText, QColor(233, 221, 201));
        pal.setColor(QPalette::Highlight, QColor(124, 52, 170));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        view->setPalette(pal);
    }
    instancesLayout->addWidget(view, 1);
    m_obsPages->addWidget(instancesPage);

    m_obsNews = new QTextBrowser();
    m_obsNews->setOpenExternalLinks(true);
    m_obsNews->setFrameShape(QFrame::NoFrame);
    m_obsNews->viewport()->setAutoFillBackground(false);
    m_obsPages->addWidget(m_obsNews);
    // the developer's modpack: a picture and a few words, nothing to choose
    m_obsDeveloper = new DeveloperPage();
    m_obsDeveloper->setPreview(QImage(QStringLiteral(":/obsidium/polytech_preview.png")));
    m_obsDeveloper->setTitle(QStringLiteral("PolyTech 1.20.1"));
    m_obsDeveloper->setDescription(
        tr("Технологическая PvP сборка для игры с друзьями: полноценный Create с аддонами, AE2, модифицированный TacZ, "
           "новые локации, сложные данжи, боссы и невероятная графика!"));
    m_obsPages->addWidget(m_obsDeveloper);
    m_obsidium->place(m_obsPages, Slot::Content);

    // ---- right column
    m_obsNick = new QLabel();
    m_obsNick->setAlignment(Qt::AlignCenter);
    m_obsNick->setStyleSheet("color: #e9ddc9; background: transparent;");
    m_obsidium->place(m_obsNick, Slot::Nick);

    m_obsSkin = new SkinPreview3D();
    m_obsSkin->setBackground(Qt::transparent);
    m_obsSkin->setToolTip(tr("Перетащите, чтобы повернуть. Двойной щелчок — сменить скин."));
    m_obsSkin->installEventFilter(this);
    m_obsidium->place(m_obsSkin, Slot::Skin);

    auto* profileButton = m_obsidium->addButton(tr("Настройки профиля"), Style::Plain, Slot::Profile, 19);
    m_obsProfile = profileButton;
    connect(profileButton, &QAbstractButton::clicked, ui->actionManageAccounts, &QAction::trigger);

    auto* playButton = m_obsidium->addButton(tr("Играть"), Style::Play, Slot::Play, 40);
    connect(playButton, &QAbstractButton::clicked, this, [this] {
        // "Играть" while the game is not running, "Остановить" while it is
        if (m_selectedInstance && m_selectedInstance->isRunning()) {
            if (ObsidiumDialog::confirm(this, tr("Остановить игру?"),
                                        tr("Игра будет закрыта сразу, несохранённый прогресс может потеряться."), tr("Остановить"),
                                        tr("Продолжить играть")))
                ui->actionKillInstance->trigger();
        } else {
            ui->actionLaunchInstance->trigger();
        }
    });
    m_obsPlay = playButton;
    // nothing to play with until there is a nick (see updateObsidiumAccount)
    connect(ui->actionLaunchInstance, &QAction::changed, this, &MainWindow::updateObsidiumAccount);
    connect(APPLICATION->accounts(), &AccountList::listChanged, this, &MainWindow::updateObsidiumAccount);

    connect(m_obsidium, &ObsidiumFrame::scaleChanged, this, &MainWindow::updateObsidiumFonts);

    // A first size for the screen it will most likely open on; showEvent() corrects it.
    resizeObsidiumWindow();
    updateObsidiumFonts(m_obsidium->scale());

    // the instance played last time is chosen again, so that "Играть" is all it takes
    const QString last = APPLICATION->settings()->get("LastLaunchedInstance").toString();
    if (!last.isEmpty() && !InstanceProxyModel::isDeveloperInstance(last) && APPLICATION->instances()->getInstanceById(last)) {
        showObsidiumPage(1);
        // (the page may have chosen it already: selecting it twice would reload it twice)
        if (!m_selectedInstance || m_selectedInstance->id() != last)
            setSelectedInstanceById(last);
    } else {
        showObsidiumPage(0);
    }
    updateObsidiumAccount();
    updateObsidiumSkin();
    updateObsidiumNews();
}

void MainWindow::hideClassicChrome()
{
    menuBar()->hide();
    ui->mainToolBar->hide();
    ui->newsToolBar->hide();
    ui->instanceToolBar->hide();
    statusBar()->hide();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (!m_obsidium || m_obsSized)
        return;
    m_obsSized = true;
    // Application restores the classic toolbars and geometry before showing the window.
    hideClassicChrome();
    resizeObsidiumWindow();
    // always open in the middle of the screen the mouse is on
    QScreen* target = QGuiApplication::screenAt(QCursor::pos());
    if (!target)
        target = screen();
    if (target)
        move(target->availableGeometry().center() - rect().center());
}

void MainWindow::resizeObsidiumWindow()
{
    const QPoint center = geometry().center();
    setFixedSize(m_obsidium->sizeForScale(m_obsidium->fittingScale(this, APPLICATION->settings()->get("ObsidiumScale").toDouble())));
    if (!isVisible())
        return;
    // a picture of another size keeps the window where it was, but on the screen
    move(center - rect().center());
    if (auto* screen = this->screen()) {
        const QRect available = screen->availableGeometry();
        if (!available.contains(frameGeometry()))
            move(available.center() - rect().center());
    }
}

void MainWindow::applyObsidiumTheme()
{
    if (!m_obsidium)
        return;
    m_obsidium->setTheme(Obsidium::theme(APPLICATION->settings()->get("ObsidiumTheme").toString()));
    resizeObsidiumWindow();
}

void MainWindow::showObsidiumPage(int page)
{
    if (!m_obsidium)
        return;
    for (int i = 0; i < m_obsTabs.size(); i++)
        m_obsTabs[i]->setChecked(i == page);

    if (page == 2) {
        m_obsPages->setCurrentIndex(1);
        if (!m_newsChecker->isLoadingNews() && m_newsChecker->getNewsEntries().isEmpty())
            m_newsChecker->reloadNews();
        updateObsidiumNews();
        return;
    }
    if (page == 0) {
        // the developer's modpack is chosen by itself: "Играть" starts it
        m_obsPages->setCurrentWidget(m_obsDeveloper);
        proxymodel->setFilter(InstanceProxyModel::Filter::Developer);
        const auto first = proxymodel->index(0, 0);
        if (first.isValid())
            view->setCurrentIndex(first);
        return;
    }
    m_obsPages->setCurrentIndex(0);
    proxymodel->setFilter(InstanceProxyModel::Filter::User);
    selectFirstVisibleInstance();
}

void MainWindow::selectFirstVisibleInstance()
{
    const auto current = view->selectionModel()->currentIndex();
    if (current.isValid() && m_selectedInstance)
        return;
    const auto first = proxymodel->index(0, 0);
    if (first.isValid())
        view->selectionModel()->setCurrentIndex(first, QItemSelectionModel::ClearAndSelect);
}

void MainWindow::updateObsidiumAccount()
{
    if (!m_obsNick)
        return;
    const bool noNicks = APPLICATION->accounts()->count() == 0;
    const bool running = m_selectedInstance && m_selectedInstance->isRunning();
    if (m_obsPlay) {
        m_obsPlay->setText(running ? tr("Остановить") : tr("Играть"));
        m_obsPlay->setDanger(running);
        m_obsPlay->setEnabled(running || (!noNicks && ui->actionLaunchInstance->isEnabled()));
        m_obsPlay->setToolTip(noNicks && !running ? tr("Сначала добавьте ник в настройках профиля") : QString());
    }
    if (m_obsProfile)
        m_obsProfile->setAttention(noNicks);
    const auto account = APPLICATION->accounts()->defaultAccount();
    if (!account) {
        m_obsNick->setText(tr("Нет учётной записи"));
        return;
    }
    const QString name = account->profileName();
    m_obsNick->setText(name.isEmpty() ? account->displayName() : name);
    updateObsidiumSkin();
}

void MainWindow::updateObsidiumSkin()
{
    if (!m_obsSkin)
        return;
    if (!m_selectedInstance) {
        m_obsInstanceName->clear();
        m_obsSkin->clearSkin();
        return;
    }
    m_obsInstanceName->setText(m_selectedInstance->name());
    // The nick's skin (its own or Steve; it goes into the instance when the game starts).
    QImage skin;
    bool slim = false;
    const auto account = APPLICATION->accounts()->defaultAccount();
    if (account ? PolytechSkins::effective(account->profileName(), skin, slim)
                : PolytechSkins::read(PolytechSkins::instanceDir(m_selectedInstance->gameRoot()), skin, slim))
        m_obsSkin->setSkin(skin, slim);
    else
        m_obsSkin->clearSkin();
}

void MainWindow::updateObsidiumNews()
{
    if (!m_obsNews)
        return;
    if (m_newsChecker->isLoadingNews()) {
        m_obsNews->setHtml(QStringLiteral("<p>%1</p>").arg(tr("Загрузка новостей...")));
        return;
    }
    const auto entries = m_newsChecker->getNewsEntries();
    if (entries.isEmpty()) {
        m_obsNews->setHtml(QStringLiteral("<p>%1</p>").arg(tr("Новостей пока нет.")));
        return;
    }
    QString html;
    for (const auto& entry : entries) {
        html += QStringLiteral("<h3 style=\"color:#d678ff\">%1</h3>").arg(entry->title.toHtmlEscaped());
        html += entry->content;
        if (!entry->link.isEmpty())
            html += QStringLiteral("<p><a style=\"color:#d678ff\" href=\"%1\">%2</a></p>").arg(entry->link.toHtmlEscaped(), tr("Подробнее"));
        html += QStringLiteral("<hr>");
    }
    m_obsNews->setHtml(html);
}

void MainWindow::updateObsidiumFonts(qreal scale)
{
    if (!m_obsidium)
        return;
    m_obsNick->setFont(Obsidium::pixelFont(20 * scale));
    m_obsInstanceName->setFont(Obsidium::pixelFont(17 * scale));
    m_obsActions->setFont(Obsidium::pixelFont(14 * scale));
}

QList<MainWindow::LegacyGroup> MainWindow::legacyActionGroups()
{
    // Rename and the icon were buttons of the old toolbar, not actions.
    if (!m_legacyRename) {
        m_legacyRename = new QAction(tr("Переименовать"), this);
        connect(m_legacyRename, &QAction::triggered, this, &MainWindow::on_actionRenameInstance_triggered);
        m_legacyIcon = new QAction(tr("Сменить значок"), this);
        connect(m_legacyIcon, &QAction::triggered, this, &MainWindow::on_actionChangeInstIcon_triggered);
    }
    m_legacyRename->setEnabled(m_selectedInstance != nullptr);
    m_legacyIcon->setEnabled(m_selectedInstance != nullptr);

    const QString instance = m_selectedInstance ? m_selectedInstance->name() : tr("не выбрана");
    QList<LegacyGroup> groups;
    groups.append({ tr("Выбранная сборка: %1").arg(instance),
                    { ui->actionLaunchInstance, ui->actionKillInstance, m_legacyRename, m_legacyIcon, ui->actionEditInstance,
                      ui->actionChangeInstGroup, ui->actionViewSelectedInstFolder, ui->actionExportInstance, ui->actionCopyInstance,
                      ui->actionDeleteInstance, ui->actionUndoTrashInstance, ui->actionCreateInstanceShortcut, ui->actionChangeSkin } });
    groups.append({ tr("Учётные записи и вид"), { ui->accountsMenu->menuAction(), ui->actionChangeTheme } });
    QList<QAction*> help;
    for (auto* action : ui->helpMenu->actions()) {
        if (!action->isSeparator() && action->isVisible())
            help.append(action);
    }
    if (ui->actionMoreNews->isVisible() && !help.contains(ui->actionMoreNews))
        help.append(ui->actionMoreNews);
    groups.append({ tr("Справка и новости"), help });
    return groups;
}

