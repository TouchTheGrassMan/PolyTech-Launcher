// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPointer>
#include <QWidget>

#include "horizon/Horizon.h"

class QCheckBox;
class QLabel;
class QNetworkReply;
class QPushButton;
class QTreeWidget;

/// The "Horizon" tab of the new instance dialog: the Minecraft versions Horizon plays, each
/// with the newest Horizon build for it, read from Horizon's GitHub releases.
class HorizonVersionsWidget : public QWidget {
    Q_OBJECT
   public:
    explicit HorizonVersionsWidget(QWidget* parent = nullptr);
    ~HorizonVersionsWidget() override = default;

    /// Loads the list the first time it is shown.
    void ensureLoaded();

    bool hasSelection() const;
    QString selectedMinecraftVersion() const;
    Horizon::Release selectedRelease() const;
    bool autoUpdate() const;

   signals:
    void selectionChanged();

   private:
    struct Row {
        Horizon::Release release;
        QString minecraft;
        bool launchApi = false;  // can be started from this launcher
    };

    void refresh();
    void onReleases(const QByteArray& json);
    void loadPins(int index);
    void probeLaunchApi(int index);
    void fillList();
    void setStatus(const QString& text);

   private:
    QLabel* m_about = nullptr;
    QTreeWidget* m_list = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_showLabel = nullptr;
    QCheckBox* m_nightly = nullptr;
    QCheckBox* m_autoUpdate = nullptr;
    QPushButton* m_refresh = nullptr;

    bool m_loaded = false;
    QList<Row> m_rows;
    QPointer<QNetworkReply> m_reply;
};
