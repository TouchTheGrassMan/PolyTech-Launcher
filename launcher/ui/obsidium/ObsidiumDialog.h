// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QDialog>
#include <QPixmap>

class QLabel;
class QPainter;
class QVBoxLayout;

/// Obsidium: the look of the launcher's own windows - dark panels, bronze frames, the purple of
/// obsidian and the pixel font of the main window.
namespace ObsidiumStyle {

inline const QColor WINDOW_BG(13, 11, 24);
inline const QColor PANEL_BG(5, 8, 20);
inline const QColor BRONZE(150, 98, 58);
inline const QColor MUTED(150, 140, 160);
inline const QColor WARNING(255, 184, 77);

inline constexpr int TEXT_PX = 12;
inline constexpr int SMALL_PX = 10;

QString rgb(const QColor& c);
/// "font-family: ...; font-size: ...px;" of the pixel font
QString fontCss(int px);
QString buttonCss();
/// The button the window wants pressed: bright obsidian purple.
QString accentButtonCss();
QString lineEditCss();
QString listCss();
QString radioCss();
/// A panel: the dark inside of a bronze frame (for a widget with this object name).
QString panelCss(const QString& objectName);
/// The accented hint at the top of a window.
QString hintCss();
QString labelCss(const QColor& color, int px);
/// Tooltips as in the game and menus in the launcher's look, for the whole application.
QString popupCss();
/// Progress bars and scroll bars of a window in the launcher's look.
QString progressCss();
/// The frame of a launcher window (the rounded dark panel with the bronze edge).
void paintWindowFrame(QPainter& painter, const QRect& rect);
/// The face of a skin (with the hat layer).
QPixmap faceOf(const QImage& skin, int size);

}  // namespace ObsidiumStyle

/// A small drawn button of the profile window: a sign instead of a caption.
class ObsidiumIconButton : public QAbstractButton {
    Q_OBJECT
   public:
    enum class Icon { Plus, Refresh, Delete, Close, Check };
    ObsidiumIconButton(Icon icon, QWidget* parent = nullptr);
    QSize sizeHint() const override { return { 22, 22 }; }

   protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

   private:
    Icon m_icon;
    bool m_hover = false;
};

/// A window of the launcher in the style of the main one: no system frame, a title with a
/// close button, moved by any free spot.
class ObsidiumDialog : public QDialog {
    Q_OBJECT
   public:
    explicit ObsidiumDialog(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    /// A question in the launcher's look; true for `yes`.
    static bool confirm(QWidget* parent, const QString& title, const QString& text, const QString& yes, const QString& no);
    /// Where the content goes, under the title.
    QVBoxLayout* body() const { return m_body; }

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

   private:
    QLabel* m_title;
    QVBoxLayout* m_body;
};
