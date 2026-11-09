// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QListWidget>

#include "ui/obsidium/ObsidiumDialog.h"

class AccountList;
class BaseInstance;
class QLabel;
class QLineEdit;
class QPushButton;
class SkinPreview3D;

/// The nicks; tells when one was dragged to another place.
class ProfileListWidget : public QListWidget {
    Q_OBJECT
   public:
    using QListWidget::QListWidget;

   signals:
    void reordered();

   protected:
    void dropEvent(QDropEvent* event) override;
};

/// Obsidium: "Настройки профиля" - the nicks to play with and their skins.
/// Opened only by the "Настройки профиля" button (and when the game is started without a nick).
class ProfileDialog : public ObsidiumDialog {
    Q_OBJECT
   public:
    /// `instance`: whose skin is shown for a nick without a skin of its own; may be null.
    explicit ProfileDialog(BaseInstance* instance, QWidget* parent = nullptr);

   private:
    void scheduleRebuild();
    void rebuild();
    void applyOrder();
    void addOffline();
    void addMicrosoft();
    void refreshAccount(const QString& internalId);
    void removeAccount(const QString& internalId);
    void makeDefault(const QString& internalId);
    void changeSkin();
    void updateSkin();
    QString selectedId() const;
    int rowOf(const QString& internalId) const;

    AccountList* m_accounts;
    BaseInstance* m_instance;
    bool m_rebuildQueued = false;

    QLineEdit* m_nickEdit;
    ObsidiumIconButton* m_addButton;
    ProfileListWidget* m_list;
    QPushButton* m_microsoftButton;
    QLabel* m_skinNick;
    SkinPreview3D* m_skin;
    QLabel* m_skinNote;
    QPushButton* m_changeSkinButton;
    QString m_selectedId;
};
