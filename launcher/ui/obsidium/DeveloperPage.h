// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QImage>
#include <QWidget>

/// Obsidium: the "От разработчика" page - the developer's modpack as a picture with a short
/// description in its lower left corner (the modpack is selected by itself: "Играть" starts it).
class DeveloperPage : public QWidget {
    Q_OBJECT
   public:
    explicit DeveloperPage(QWidget* parent = nullptr);

    void setPreview(const QImage& preview);
    void setTitle(const QString& title);
    void setDescription(const QString& description);

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    QImage m_preview;
    QString m_title;
    QString m_description;
};
