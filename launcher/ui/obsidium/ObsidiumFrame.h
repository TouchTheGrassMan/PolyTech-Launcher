// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Obsidium - the drawn main window.
 *
 *  The window is a picture (a theme: resources/obsidium/frame*.png) with widgets laid over its
 *  painted buttons and panels. Each theme says where its buttons and panels are, in the
 *  picture's own coordinates; widgets are placed into these slots and follow the theme when
 *  it changes. Sizes of captions and fonts are given for a window REFERENCE_WIDTH wide, so
 *  every theme looks the same size at the same scale.
 */

#pragma once

#include <QAbstractButton>
#include <QFont>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QRect>
#include <QWidget>

class QVariantAnimation;

namespace Obsidium {

/// Window width at scale 1.0; caption sizes are given for it.
inline constexpr int REFERENCE_WIDTH = 1570;

/// The places of a picture that widgets go to.
enum class Slot {
    Minimize,
    Close,
    Left1,
    Left2,
    Left3,
    Left4,
    Left5,
    Tab1,
    Tab2,
    Tab3,
    Content,
    Nick,
    Skin,
    Profile,
    Play,
    Count
};

/// One look of the window: a picture and where its slots are on it.
struct Theme {
    QString id;
    QString name;
    QString picture;
    QSize base;  // the size the rectangles were measured on
    bool available = true;  // offered in the settings (a seasonal one is switched off out of season)
    QRect rects[int(Slot::Count)];

    QRect rect(Slot slot) const { return rects[int(slot)]; }
};

/// All themes; the first one is the default.
const QList<Theme>& themes();
/// The theme with this id, or the default one (also for a theme that is switched off).
const Theme& theme(const QString& id);

/// The font of the picture's captions ("Minecraft Rus"), loaded from the resources once.
QFont pixelFont(qreal pixelSize);

/// Light text on the dark stone of the picture.
inline const QColor TEXT_COLOR(233, 221, 201);
inline const QColor ACCENT_COLOR(214, 120, 255);

}  // namespace Obsidium

/// Text (or nothing) drawn over a button that is already part of the picture.
class ObsidiumButton : public QAbstractButton {
    Q_OBJECT
   public:
    enum class Style {
        Plain,  // the long stone buttons
        Tab,    // the page tabs: checkable, the open one glows
        Play,   // the big crystal button
        Bare,   // the window buttons: the picture already has the sign
    };

    ObsidiumButton(const QString& text, Style style, QWidget* parent = nullptr);

    /// Caption size in the picture's pixels.
    void setBasePixelSize(qreal px) { m_basePx = px; }
    void setScale(qreal scale);
    /// Pulses to draw the eye (e.g. "Настройки профиля" while there is no nick).
    void setAttention(bool attention);
    /// Red instead of purple ("Остановить" while the game runs).
    void setDanger(bool danger);

    QSize sizeHint() const override { return { 100, 30 }; }

   protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

   private:
    Style m_style;
    qreal m_basePx = 22;
    qreal m_scale = 1.0;
    bool m_hover = false;
    QVariantAnimation* m_pulse = nullptr;
    qreal m_pulseLevel = 0;
    bool m_danger = false;
};

/// The window's picture, with the widgets placed on it.
class ObsidiumFrame : public QWidget {
    Q_OBJECT
   public:
    explicit ObsidiumFrame(QWidget* parent = nullptr);

    /// Changes the picture; the placed widgets move to the new theme's slots.
    void setTheme(const Obsidium::Theme& theme);
    const Obsidium::Theme& theme() const { return m_theme; }

    /// Puts a widget into a slot of the picture.
    void place(QWidget* widget, Obsidium::Slot slot);
    /// A button in a slot of the picture.
    ObsidiumButton* addButton(const QString& text, ObsidiumButton::Style style, Obsidium::Slot slot, qreal basePixelSize);

    /// Window size for a scale (1.0 = REFERENCE_WIDTH wide).
    QSize sizeForScale(qreal scale) const;
    /// `preferred`, made smaller when the screen of `widget` is too small for it and never
    /// larger than the picture's resolution allows.
    qreal fittingScale(const QWidget* widget, qreal preferred) const;

    /// Width of the window relative to REFERENCE_WIDTH: the scale of captions and fonts.
    qreal scale() const { return m_scale; }

    /// Darkens the picture while a dialog is open over it.
    void setDimmed(bool dimmed);
    /// Dims the picture by itself whenever a dialog of the launcher is open (except `mainWindow`).
    void dimForDialogs(QWidget* mainWindow);

   signals:
    void scaleChanged(qreal scale);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

   private:
    void relayout();

    struct Placed {
        QPointer<QWidget> widget;
        Obsidium::Slot slot;
    };

    Obsidium::Theme m_theme;
    QImage m_picture;
    QWidget* m_dim = nullptr;
    QList<Placed> m_placed;
    qreal m_scale = 1.0;
};
