// SPDX-License-Identifier: GPL-3.0-only
#include "ProfileDialog.h"

#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include "Application.h"
#include "BaseInstance.h"
#include "PolytechSkins.h"
#include "minecraft/auth/AccountList.h"
#include "ui/dialogs/ChangeSkinDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "ui/dialogs/SkinPreview3D.h"
#include "ui/obsidium/ObsidiumFrame.h"

using namespace ObsidiumStyle;

// ---- ProfileListWidget --------------------------------------------------------------------

void ProfileListWidget::dropEvent(QDropEvent* event)
{
    QListWidget::dropEvent(event);
    emit reordered();
}

// ---- ProfileDialog ------------------------------------------------------------------------

ProfileDialog::ProfileDialog(BaseInstance* instance, QWidget* parent)
    : ObsidiumDialog(parent), m_accounts(APPLICATION->accounts()), m_instance(instance)
{
    setTitle(tr("Настройки профиля"));
    setFixedSize(574, 392);
    auto* outer = body();

    // ---- the hint, in the accent colour
    auto* hint = new QLabel(tr("<b>Регистрация не требуется.</b> Введите ник, чтобы войти в игру.<br>"
                               "Вы также можете воспользоваться авторизацией через Microsoft, если у вас есть лицензионная копия "
                               "Minecraft, но это необязательно."),
                            this);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setStyleSheet(hintCss());
    outer->addWidget(hint);

    auto* body = new QHBoxLayout();
    body->setSpacing(14);
    outer->addLayout(body, 1);

    // ---- left: the nick field, the nicks, Microsoft
    auto* left = new QVBoxLayout();
    left->setSpacing(7);
    auto* addRow = new QHBoxLayout();
    addRow->setSpacing(6);
    m_nickEdit = new QLineEdit(this);
    m_nickEdit->setPlaceholderText(tr("Ваш ник"));
    m_nickEdit->setMaxLength(16);
    m_nickEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[A-Za-z0-9_]{0,16}$")), this));
    m_nickEdit->setToolTip(tr("От 3 до 16 символов: латинские буквы, цифры и _"));
    m_nickEdit->setStyleSheet(QStringLiteral("QLineEdit { %1 color: %2; background: %3; border: 2px solid %4; border-radius: 5px; "
                                             "padding: 3px 6px; selection-background-color: %5; }"
                                             "QLineEdit:focus { border-color: %5; }")
                                  .arg(fontCss(TEXT_PX), rgb(Obsidium::TEXT_COLOR), rgb(PANEL_BG), rgb(BRONZE),
                                       rgb(Obsidium::ACCENT_COLOR)));
    addRow->addWidget(m_nickEdit, 1);
    m_addButton = new ObsidiumIconButton(ObsidiumIconButton::Icon::Plus, this);
    m_addButton->setFixedSize(26, 26);
    m_addButton->setToolTip(tr("Добавить ник"));
    addRow->addWidget(m_addButton);
    left->addLayout(addRow);

    m_list = new ProfileListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setSpacing(1);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background: %1; border: 2px solid %2; border-radius: 6px; padding: 3px; outline: none; }"
                                         "QListWidget::item { border-radius: 6px; }"
                                         "QListWidget::item:selected { background: rgba(214, 120, 255, 45); }"
                                         "QListWidget::item:hover:!selected { background: rgba(214, 120, 255, 20); }")
                              .arg(rgb(PANEL_BG), rgb(BRONZE)));
    left->addWidget(m_list, 1);

    auto* dragHint = new QLabel(tr("Ники можно перетаскивать. Галочка — ник, с которым запускается игра."), this);
    dragHint->setWordWrap(true);
    dragHint->setStyleSheet(QStringLiteral("color: %1; %2").arg(rgb(MUTED), fontCss(SMALL_PX)));
    left->addWidget(dragHint);

    m_microsoftButton = new QPushButton(tr("Войти через Microsoft"), this);
    m_microsoftButton->setStyleSheet(buttonCss());
    m_microsoftButton->setCursor(Qt::PointingHandCursor);
    // Enter in the nick field adds the nick, it must not press a button of the dialog
    m_microsoftButton->setAutoDefault(false);
    if (!APPLICATION->capabilities().testFlag(Application::SupportsMSA)) {
        // Obsidium has no Microsoft app of its own yet (Launcher_MSA_CLIENT_ID is empty)
        m_microsoftButton->setEnabled(false);
        m_microsoftButton->setToolTip(tr("Вход через Microsoft временно невозможен"));
    }
    left->addWidget(m_microsoftButton);
    body->addLayout(left, 3);

    // ---- right: the skin of the selected nick
    auto* right = new QVBoxLayout();
    right->setSpacing(7);
    m_skinNick = new QLabel(this);
    m_skinNick->setAlignment(Qt::AlignCenter);
    m_skinNick->setStyleSheet(QStringLiteral("QLabel { %1 color: %2; background: %3; border: 2px solid %4; border-radius: 6px; "
                                             "padding: 4px; }")
                                  .arg(fontCss(13), rgb(Obsidium::TEXT_COLOR), rgb(PANEL_BG), rgb(BRONZE)));
    right->addWidget(m_skinNick);

    auto* skinFrame = new QWidget(this);
    skinFrame->setObjectName(QStringLiteral("skinFrame"));
    skinFrame->setStyleSheet(QStringLiteral("#skinFrame { background: %1; border: 2px solid %2; border-radius: 6px; }")
                                 .arg(rgb(PANEL_BG), rgb(BRONZE)));
    auto* skinLayout = new QVBoxLayout(skinFrame);
    skinLayout->setContentsMargins(4, 4, 4, 4);
    m_skin = new SkinPreview3D(skinFrame);
    m_skin->setBackground(Qt::transparent);
    skinLayout->addWidget(m_skin, 1);
    m_skinNote = new QLabel(skinFrame);
    m_skinNote->setAlignment(Qt::AlignCenter);
    m_skinNote->setWordWrap(true);
    m_skinNote->setStyleSheet(QStringLiteral("color: %1; %2 background: transparent; border: none;").arg(rgb(MUTED), fontCss(SMALL_PX)));
    skinLayout->addWidget(m_skinNote);
    right->addWidget(skinFrame, 1);

    m_changeSkinButton = new QPushButton(tr("Сменить скин"), this);
    m_changeSkinButton->setStyleSheet(buttonCss());
    m_changeSkinButton->setCursor(Qt::PointingHandCursor);
    m_changeSkinButton->setAutoDefault(false);
    right->addWidget(m_changeSkinButton);
    body->addLayout(right, 2);

    connect(m_addButton, &QAbstractButton::clicked, this, &ProfileDialog::addOffline);
    connect(m_nickEdit, &QLineEdit::returnPressed, this, &ProfileDialog::addOffline);
    connect(m_nickEdit, &QLineEdit::textChanged, this, [this](const QString& text) { m_addButton->setEnabled(text.length() >= 3); });
    m_addButton->setEnabled(false);
    connect(m_microsoftButton, &QPushButton::clicked, this, &ProfileDialog::addMicrosoft);
    connect(m_changeSkinButton, &QPushButton::clicked, this, &ProfileDialog::changeSkin);
    connect(m_list, &ProfileListWidget::reordered, this, &ProfileDialog::applyOrder);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) {
        m_selectedId = selectedId();
        updateSkin();
    });

    connect(m_accounts, &AccountList::listChanged, this, &ProfileDialog::scheduleRebuild);
    connect(m_accounts, &AccountList::listActivityChanged, this, &ProfileDialog::scheduleRebuild);
    connect(m_accounts, &AccountList::defaultAccountChanged, this, &ProfileDialog::scheduleRebuild);

    if (auto account = m_accounts->defaultAccount())
        m_selectedId = account->internalId();
    rebuild();
    m_nickEdit->setFocus();
}

QString ProfileDialog::selectedId() const
{
    auto* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

int ProfileDialog::rowOf(const QString& internalId) const
{
    for (int i = 0; i < m_accounts->count(); i++) {
        if (m_accounts->at(i)->internalId() == internalId)
            return i;
    }
    return -1;
}

void ProfileDialog::scheduleRebuild()
{
    // The list may change from a button of a row: the rows are rebuilt once that click is over.
    if (m_rebuildQueued)
        return;
    m_rebuildQueued = true;
    QTimer::singleShot(0, this, &ProfileDialog::rebuild);
}

void ProfileDialog::rebuild()
{
    m_rebuildQueued = false;
    const QString keep = m_selectedId;
    const auto defaultAccount = m_accounts->defaultAccount();

    m_list->blockSignals(true);
    m_list->clear();
    for (int i = 0; i < m_accounts->count(); i++) {
        const auto account = m_accounts->at(i);
        const QString id = account->internalId();
        const bool online = account->accountType() != AccountType::Offline;

        auto* row = new QWidget();
        row->setAttribute(Qt::WA_TranslucentBackground);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(4, 3, 4, 3);
        layout->setSpacing(7);

        auto* check = new ObsidiumIconButton(ObsidiumIconButton::Icon::Check, row);
        check->setChecked(account == defaultAccount);
        check->setToolTip(tr("Играть с этим ником"));
        connect(check, &QAbstractButton::clicked, this, [this, id] { makeDefault(id); });
        layout->addWidget(check);

        auto* face = new QLabel(row);
        face->setFixedSize(16, 16);
        QImage skin;
        bool slim = false;
        if (PolytechSkins::effective(account->profileName(), skin, slim))
            face->setPixmap(faceOf(skin, 16));
        face->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(face);

        auto* texts = new QVBoxLayout();
        texts->setSpacing(0);
        auto* nick = new QLabel(account->profileName().isEmpty() ? account->displayName() : account->profileName(), row);
        nick->setStyleSheet(QStringLiteral("color: %1; %2 background: transparent;").arg(rgb(Obsidium::TEXT_COLOR), fontCss(TEXT_PX)));
        nick->setAttribute(Qt::WA_TransparentForMouseEvents);
        texts->addWidget(nick);
        if (online) {
            const QString type = m_accounts->data(m_accounts->index(i, AccountList::TypeColumn), Qt::DisplayRole).toString();
            const QString status = m_accounts->data(m_accounts->index(i, AccountList::StatusColumn), Qt::DisplayRole).toString();
            auto* info = new QLabel(status.isEmpty() ? type : QStringLiteral("%1 · %2").arg(type, status), row);
            info->setStyleSheet(QStringLiteral("color: %1; %2 background: transparent;").arg(rgb(MUTED), fontCss(SMALL_PX)));
            info->setAttribute(Qt::WA_TransparentForMouseEvents);
            texts->addWidget(info);
        }
        layout->addLayout(texts, 1);

        if (online) {
            auto* refresh = new ObsidiumIconButton(ObsidiumIconButton::Icon::Refresh, row);
            refresh->setToolTip(tr("Обновить вход"));
            refresh->setEnabled(!account->isActive());
            connect(refresh, &QAbstractButton::clicked, this, [this, id] { refreshAccount(id); });
            layout->addWidget(refresh);
        }
        auto* remove = new ObsidiumIconButton(ObsidiumIconButton::Icon::Delete, row);
        remove->setToolTip(tr("Удалить ник"));
        remove->setEnabled(!account->isActive());
        connect(remove, &QAbstractButton::clicked, this, [this, id] { removeAccount(id); });
        layout->addWidget(remove);

        auto* item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, id);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        item->setSizeHint(QSize(0, online ? 36 : 30));
        m_list->setItemWidget(item, row);
        if (id == keep)
            m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem() && m_list->count() > 0)
        m_list->setCurrentRow(0);
    m_list->blockSignals(false);
    m_selectedId = selectedId();
    updateSkin();
}

void ProfileDialog::applyOrder()
{
    // the order the nicks were dragged into
    QStringList order;
    for (int i = 0; i < m_list->count(); i++)
        order << m_list->item(i)->data(Qt::UserRole).toString();
    m_selectedId = selectedId();
    for (int target = 0; target < order.size(); target++) {
        int row = rowOf(order[target]);
        while (row > target) {
            m_accounts->moveAccount(m_accounts->index(row, 0), -1);
            row--;
        }
    }
    scheduleRebuild();
}

void ProfileDialog::addOffline()
{
    const QString nick = m_nickEdit->text().trimmed();
    if (nick.length() < 3)
        return;
    for (int i = 0; i < m_accounts->count(); i++) {
        const auto existing = m_accounts->at(i);
        if (existing->accountType() == AccountType::Offline && existing->profileName().compare(nick, Qt::CaseInsensitive) == 0) {
            // already there: just point at it
            m_selectedId = existing->internalId();
            m_nickEdit->clear();
            rebuild();
            return;
        }
    }
    if (const auto account = MinecraftAccount::createOffline(nick)) {
        account->login()->start();  // offline: done at once
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1 || !m_accounts->defaultAccount())
            m_accounts->setDefaultAccount(account);
        m_selectedId = account->internalId();
        m_nickEdit->clear();
        scheduleRebuild();
    }
}

void ProfileDialog::addMicrosoft()
{
    if (const auto account = MSALoginDialog::newAccount(this)) {
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1 || !m_accounts->defaultAccount())
            m_accounts->setDefaultAccount(account);
        m_selectedId = account->internalId();
        scheduleRebuild();
    }
}

void ProfileDialog::refreshAccount(const QString& internalId)
{
    m_accounts->requestRefresh(internalId);
}

void ProfileDialog::removeAccount(const QString& internalId)
{
    const int row = rowOf(internalId);
    if (row < 0)
        return;
    const auto account = m_accounts->at(row);
    const QString name = account->profileName().isEmpty() ? account->displayName() : account->profileName();
    if (!ObsidiumDialog::confirm(this, tr("Удалить ник?"), tr("Удалить ник %1 из лаунчера?").arg(name), tr("Удалить"), tr("Оставить")))
        return;
    const int current = rowOf(internalId);  // the list may have changed while asking
    if (current >= 0)
        m_accounts->removeAccount(m_accounts->index(current, 0));
    scheduleRebuild();
}

void ProfileDialog::makeDefault(const QString& internalId)
{
    const int row = rowOf(internalId);
    if (row < 0)
        return;
    m_accounts->setDefaultAccount(m_accounts->at(row));
    m_selectedId = internalId;
    scheduleRebuild();
}

void ProfileDialog::updateSkin()
{
    const int row = rowOf(m_selectedId);
    if (row < 0) {
        m_skinNick->setText(tr("Нет ника"));
        m_skin->clearSkin();
        m_skinNote->setText(tr("Добавьте ник слева"));
        m_changeSkinButton->setEnabled(false);
        return;
    }
    const auto account = m_accounts->at(row);
    const QString nick = account->profileName().isEmpty() ? account->displayName() : account->profileName();
    m_skinNick->setText(nick);
    m_changeSkinButton->setEnabled(true);

    QImage skin;
    bool slim = false;
    bool isDefault = false;
    if (PolytechSkins::effective(account->profileName(), skin, slim, &isDefault)) {
        m_skin->setSkin(skin, slim);
        m_skinNote->setText(isDefault ? tr("Стандартный скин") : tr("Покрутите мышью"));
    } else {
        m_skin->clearSkin();
        m_skinNote->setText(tr("Скин не выбран"));
    }
}

void ProfileDialog::changeSkin()
{
    const int row = rowOf(m_selectedId);
    if (row < 0)
        return;
    ChangeSkinDialog dialog(m_accounts->at(row)->profileName(), m_instance, this);
    dialog.exec();
    rebuild();  // the face in the list and the preview
}
