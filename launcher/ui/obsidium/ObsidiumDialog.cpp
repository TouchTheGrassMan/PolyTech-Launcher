// SPDX-License-Identifier: GPL-3.0-only
#include "ObsidiumDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWindow>

#include "ui/obsidium/ObsidiumFrame.h"

namespace ObsidiumStyle {


QString rgb(const QColor& c)
{
    return c.name(QColor::HexRgb);
}

QString fontCss(int px)
{
    return QStringLiteral("font-family: \"%1\"; font-size: %2px;").arg(Obsidium::pixelFont(px).family()).arg(px);
}

QString buttonCss()
{
    return QStringLiteral(
               "QPushButton { %1 color: %2; background: #1a1230; border: 2px solid %3; border-radius: 5px; padding: 5px 10px; }"
               "QPushButton:hover { border-color: %4; background: #26183f; color: #ffffff; }"
               "QPushButton:pressed { background: #3a2160; }"
               "QPushButton:disabled { color: #6e6878; border-color: #4a3a30; }")
        .arg(fontCss(TEXT_PX), rgb(Obsidium::TEXT_COLOR), rgb(BRONZE), rgb(Obsidium::ACCENT_COLOR));
}

/// The face of a skin (with the hat layer) for the list.
QPixmap faceOf(const QImage& skin, int size)
{
    if (skin.isNull() || skin.width() < 64 || skin.height() < 16)
        return {};
    QImage face = skin.copy(8, 8, 8, 8).convertToFormat(QImage::Format_ARGB32);
    QPainter p(&face);
    p.drawImage(0, 0, skin.copy(40, 8, 8, 8));
    p.end();
    return QPixmap::fromImage(face.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}


QString lineEditCss()
{
    return QStringLiteral("QLineEdit { %1 color: %2; background: %3; border: 2px solid %4; border-radius: 5px; "
                          "padding: 3px 6px; selection-background-color: %5; }"
                          "QLineEdit:focus { border-color: %5; }")
        .arg(fontCss(TEXT_PX), rgb(Obsidium::TEXT_COLOR), rgb(PANEL_BG), rgb(BRONZE), rgb(Obsidium::ACCENT_COLOR));
}

QString listCss()
{
    return QStringLiteral("QListWidget { %1 color: %2; background: %3; border: 2px solid %4; border-radius: 6px; padding: 3px; "
                          "outline: none; }"
                          "QListWidget::item { border-radius: 5px; padding: 2px; }"
                          "QListWidget::item:selected { background: rgba(214, 120, 255, 45); color: #ffffff; }"
                          "QListWidget::item:hover:!selected { background: rgba(214, 120, 255, 20); }")
        .arg(fontCss(TEXT_PX), rgb(Obsidium::TEXT_COLOR), rgb(PANEL_BG), rgb(BRONZE));
}

QString radioCss()
{
    return QStringLiteral("QRadioButton { %1 color: %2; spacing: 6px; background: transparent; }"
                          "QRadioButton:disabled { color: #6e6878; }"
                          "QRadioButton::indicator { width: 10px; height: 10px; border: 2px solid %3; border-radius: 7px; background: %4; }"
                          "QRadioButton::indicator:checked { background: %5; border-color: %5; }"
                          "QRadioButton::indicator:hover { border-color: %5; }")
        .arg(fontCss(TEXT_PX), rgb(Obsidium::TEXT_COLOR), rgb(BRONZE), rgb(PANEL_BG), rgb(Obsidium::ACCENT_COLOR));
}

QString panelCss(const QString& objectName)
{
    return QStringLiteral("#%1 { background: %2; border: 2px solid %3; border-radius: 6px; }").arg(objectName, rgb(PANEL_BG), rgb(BRONZE));
}

QString hintCss()
{
    return QStringLiteral("QLabel { %1 color: #ffffff; background: rgba(214, 120, 255, 38); "
                          "border: 2px solid %2; border-radius: 6px; padding: 7px 10px; }")
        .arg(fontCss(TEXT_PX), rgb(Obsidium::ACCENT_COLOR));
}

QString labelCss(const QColor& color, int px)
{
    return QStringLiteral("QLabel { color: %1; %2 background: transparent; border: none; }").arg(rgb(color), fontCss(px));
}

QString popupCss()
{
    // The tooltip of the game: almost black purple, a border fading from violet to dark violet.
    const QString tooltip = QStringLiteral(
                                "QToolTip { %1 color: #ffffff; background-color: #100010; padding: 3px 5px; border: 2px solid #3c00b0; "
                                "border-top-color: #5000ff; border-bottom-color: #28007f; "
                                "border-left-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5000ff, stop:1 #28007f); "
                                "border-right-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5000ff, stop:1 #28007f); }")
                                .arg(fontCss(TEXT_PX));
    const QString menu = QStringLiteral(
                             "QMenu { %1 color: %2; background-color: %3; border: 2px solid %4; padding: 4px; }"
                             "QMenu::item { padding: 5px 22px 5px 30px; border-radius: 4px; background: transparent; }"
                             "QMenu::item:selected { background-color: rgba(214, 120, 255, 60); color: #ffffff; }"
                             "QMenu::item:disabled { color: #6e6878; }"
                             "QMenu::icon { padding-left: 8px; }"
                             "QMenu::separator { height: 2px; background: #3b2a20; margin: 4px 6px; }"
                             "QMenu::indicator { width: 12px; height: 12px; padding-left: 8px; }")
                             .arg(fontCss(TEXT_PX), rgb(Obsidium::TEXT_COLOR), rgb(WINDOW_BG), rgb(BRONZE));
    return tooltip + menu;
}

}  // namespace ObsidiumStyle

using namespace ObsidiumStyle;

// ---- ObsidiumIconButton -------------------------------------------------------------------

ObsidiumIconButton::ObsidiumIconButton(Icon icon, QWidget* parent) : QAbstractButton(parent), m_icon(icon)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(sizeHint());
    if (icon == Icon::Check)
        setCheckable(true);
}

void ObsidiumIconButton::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ObsidiumIconButton::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ObsidiumIconButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    const bool on = isEnabled() && (m_hover || isDown());

    // the box
    if (m_icon != Icon::Close) {
        QPainterPath box;
        box.addRoundedRect(r, 4, 4);
        p.fillPath(box, on ? QColor(38, 24, 63) : QColor(26, 18, 48));
        p.setPen(QPen(on || (m_icon == Icon::Check && isChecked()) ? Obsidium::ACCENT_COLOR : BRONZE, 2));
        p.drawPath(box);
    }

    QColor ink = !isEnabled() ? QColor(90, 84, 96) : on ? QColor(255, 255, 255) : Obsidium::TEXT_COLOR;
    if (m_icon == Icon::Delete && on)
        ink = QColor(255, 120, 120);
    QPen pen(ink, 1.8, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    p.setPen(pen);
    const QPointF c = r.center();
    const qreal s = r.width() * 0.26;

    switch (m_icon) {
        case Icon::Plus:
            p.drawLine(QPointF(c.x() - s, c.y()), QPointF(c.x() + s, c.y()));
            p.drawLine(QPointF(c.x(), c.y() - s), QPointF(c.x(), c.y() + s));
            break;
        case Icon::Close:
            p.drawLine(QPointF(c.x() - s, c.y() - s), QPointF(c.x() + s, c.y() + s));
            p.drawLine(QPointF(c.x() - s, c.y() + s), QPointF(c.x() + s, c.y() - s));
            break;
        case Icon::Check:
            if (isChecked()) {
                p.setPen(QPen(Obsidium::ACCENT_COLOR, 2.2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
                p.drawPolyline(QPolygonF({ QPointF(c.x() - s, c.y()), QPointF(c.x() - s * 0.3, c.y() + s * 0.75),
                                           QPointF(c.x() + s, c.y() - s * 0.8) }));
            }
            break;
        case Icon::Refresh: {
            // two round arrows
            const QRectF ring(c.x() - s, c.y() - s, 2 * s, 2 * s);
            p.setBrush(Qt::NoBrush);
            p.drawArc(ring, 20 * 16, 140 * 16);
            p.drawArc(ring, 200 * 16, 140 * 16);
            p.setBrush(ink);
            p.setPen(Qt::NoPen);
            const qreal a = s * 0.55;
            // heads at the ends of the arcs (160 and 340 degrees)
            const QPointF h1(c.x() - s * 0.94, c.y() - s * 0.34);
            p.drawPolygon(QPolygonF({ h1 + QPointF(-a, -a * 0.2), h1 + QPointF(a * 0.6, -a * 0.2), h1 + QPointF(-a * 0.2, a * 0.9) }));
            const QPointF h2(c.x() + s * 0.94, c.y() + s * 0.34);
            p.drawPolygon(QPolygonF({ h2 + QPointF(a, a * 0.2), h2 + QPointF(-a * 0.6, a * 0.2), h2 + QPointF(a * 0.2, -a * 0.9) }));
            break;
        }
        case Icon::Delete: {
            // a bin
            const qreal w = s * 1.5;
            p.drawLine(QPointF(c.x() - s * 1.05, c.y() - s * 0.75), QPointF(c.x() + s * 1.05, c.y() - s * 0.75));
            p.drawLine(QPointF(c.x() - s * 0.35, c.y() - s * 1.05), QPointF(c.x() + s * 0.35, c.y() - s * 1.05));
            p.drawRect(QRectF(c.x() - w / 2, c.y() - s * 0.45, w, s * 1.5));
            p.drawLine(QPointF(c.x(), c.y() - s * 0.2), QPointF(c.x(), c.y() + s * 0.8));
            break;
        }
    }
}

// ---- ObsidiumDialog ----------------------------------------------------------------------

ObsidiumDialog::ObsidiumDialog(QWidget* parent) : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);

    m_body = new QVBoxLayout(this);
    m_body->setContentsMargins(18, 12, 18, 18);
    m_body->setSpacing(10);

    auto* titleRow = new QHBoxLayout();
    m_title = new QLabel(this);
    m_title->setFont(Obsidium::pixelFont(18));
    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(rgb(Obsidium::TEXT_COLOR)));
    titleRow->addWidget(m_title);
    titleRow->addStretch(1);
    auto* closeButton = new ObsidiumIconButton(ObsidiumIconButton::Icon::Close, this);
    closeButton->setToolTip(tr("Закрыть"));
    connect(closeButton, &QAbstractButton::clicked, this, &QDialog::accept);
    titleRow->addWidget(closeButton);
    m_body->addLayout(titleRow);
}

void ObsidiumDialog::setTitle(const QString& title)
{
    setWindowTitle(title);
    m_title->setText(title);
}

void ObsidiumDialog::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
    QPainterPath path;
    path.addRoundedRect(r, 10, 10);
    p.fillPath(path, WINDOW_BG);
    p.setPen(QPen(BRONZE, 3));
    p.drawPath(path);
    QPainterPath inner;
    inner.addRoundedRect(r.adjusted(4, 4, -4, -4), 7, 7);
    p.setPen(QPen(QColor(214, 120, 255, 110), 2));
    p.drawPath(inner);
}

void ObsidiumDialog::mousePressEvent(QMouseEvent* event)
{
    // no title bar: the window is moved by any free spot
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}
