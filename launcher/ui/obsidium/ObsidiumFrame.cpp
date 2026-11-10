// SPDX-License-Identifier: GPL-3.0-only
#include "ObsidiumFrame.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QPointer>
#include <QSet>
#include <QVariantAnimation>
#include <QWindow>

namespace Obsidium {

QFont pixelFont(qreal pixelSize)
{
    static QString s_family;
    if (s_family.isEmpty()) {  // (tried again if the resources were not there yet)
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/obsidium/minecraft.ttf"));
        const auto families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            s_family = families.first();
    }
    QFont font(s_family);
    font.setPixelSize(qMax(1, qRound(pixelSize)));
    font.setStyleStrategy(QFont::NoAntialias);  // a pixel font stays crisp
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

namespace {
Theme makeCave()
{
    Theme t;
    t.id = QStringLiteral("cave");
    t.name = QObject::tr("Пещера");
    t.picture = QStringLiteral(":/obsidium/frame_cave.png");
    t.base = { 1650, 1082 };
    auto set = [&t](Slot slot, QRect r) { t.rects[int(slot)] = r; };
    set(Slot::Minimize, { 1410, 101, 64, 60 });
    set(Slot::Close, { 1483, 101, 62, 60 });
    set(Slot::Left1, { 146, 369, 280, 54 });
    set(Slot::Left2, { 146, 435, 280, 54 });
    set(Slot::Left3, { 146, 500, 280, 54 });
    set(Slot::Left4, { 146, 567, 280, 54 });
    set(Slot::Left5, { 146, 634, 280, 54 });
    set(Slot::Tab1, { 511, 388, 201, 49 });
    set(Slot::Tab2, { 721, 388, 199, 49 });
    set(Slot::Tab3, { 929, 388, 200, 49 });
    set(Slot::Content, { 476, 444, 672, 396 });
    set(Slot::Nick, { 1192, 332, 326, 40 });
    set(Slot::Skin, { 1220, 402, 284, 270 });
    set(Slot::Profile, { 1196, 704, 332, 48 });
    set(Slot::Play, { 1196, 762, 334, 110 });
    return t;
}

Theme makeNewYear()
{
    Theme t;
    t.id = QStringLiteral("newyear");
    t.name = QObject::tr("Новогоднее");
    t.available = false;  // for the New Year holidays: switched on by setting this to true
    t.picture = QStringLiteral(":/obsidium/frame_winter.png");
    t.base = { 1650, 1082 };
    auto set = [&t](Slot slot, QRect r) { t.rects[int(slot)] = r; };
    set(Slot::Minimize, { 1412, 99, 67, 61 });
    set(Slot::Close, { 1486, 99, 66, 61 });
    set(Slot::Left1, { 145, 368, 280, 54 });
    set(Slot::Left2, { 145, 435, 280, 53 });
    set(Slot::Left3, { 145, 500, 280, 53 });
    set(Slot::Left4, { 145, 567, 280, 53 });
    set(Slot::Left5, { 145, 633, 280, 55 });
    set(Slot::Tab1, { 505, 387, 205, 46 });
    set(Slot::Tab2, { 720, 387, 198, 46 });
    set(Slot::Tab3, { 928, 387, 200, 46 });
    set(Slot::Content, { 484, 442, 658, 398 });
    set(Slot::Nick, { 1197, 340, 328, 38 });
    set(Slot::Skin, { 1222, 402, 284, 272 });
    set(Slot::Profile, { 1198, 704, 334, 48 });
    set(Slot::Play, { 1198, 762, 334, 110 });
    return t;
}
}  // namespace

const QList<Theme>& themes()
{
    static const QList<Theme> s_themes = { makeCave(), makeNewYear() };
    return s_themes;
}

const Theme& theme(const QString& id)
{
    for (const auto& t : themes()) {
        if (t.id == id && t.available)
            return t;
    }
    return themes().first();
}

}  // namespace Obsidium

// ---- ObsidiumButton -----------------------------------------------------------------------

ObsidiumButton::ObsidiumButton(const QString& text, Style style, QWidget* parent) : QAbstractButton(parent), m_style(style)
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    if (style == Style::Tab)
        setCheckable(true);
}

void ObsidiumButton::setScale(qreal scale)
{
    m_scale = scale;
    update();
}

void ObsidiumButton::setAttention(bool attention)
{
    if (attention == (m_pulse != nullptr))
        return;
    if (!attention) {
        delete m_pulse;
        m_pulse = nullptr;
        m_pulseLevel = 0;
        update();
        return;
    }
    m_pulse = new QVariantAnimation(this);
    m_pulse->setDuration(1300);
    m_pulse->setStartValue(0.0);
    m_pulse->setKeyValueAt(0.5, 1.0);
    m_pulse->setEndValue(0.0);
    m_pulse->setEasingCurve(QEasingCurve::InOutSine);
    m_pulse->setLoopCount(-1);
    connect(m_pulse, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_pulseLevel = value.toReal();
        update();
    });
    m_pulse->start();
}

void ObsidiumButton::setDanger(bool danger)
{
    m_danger = danger;
    update();
}

void ObsidiumButton::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ObsidiumButton::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ObsidiumButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(3 * m_scale, 3 * m_scale, -3 * m_scale, -3 * m_scale);
    const qreal radius = 6 * m_scale;

    const bool down = isDown();
    const bool active = isChecked();

    if (m_danger) {
        // the purple crystal turns red: tinted, the frame around it left as it is
        const qreal inset = 10 * m_scale;
        QPainterPath crystal;
        crystal.addRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset), radius, radius);
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_Multiply);
        p.fillPath(crystal, QColor(255, 80, 80));
        p.setCompositionMode(QPainter::CompositionMode_Screen);
        p.fillPath(crystal, QColor(110, 0, 0));
        p.restore();
    }

    // The picture already draws the button: only light it up.
    if (isEnabled() && (m_hover || down || active)) {
        QPainterPath path;
        path.addRoundedRect(r, radius, radius);
        QColor glow = m_style == Style::Bare ? QColor(255, 255, 255) : m_danger ? QColor(255, 90, 90) : Obsidium::ACCENT_COLOR;
        glow.setAlpha(down ? 80 : active ? 60 : 30);
        p.fillPath(path, glow);
    }
    if (isEnabled() && m_pulse && !(m_hover || down)) {
        // the call for attention: a glow coming and going, with a bright edge
        QPainterPath path;
        path.addRoundedRect(r, radius, radius);
        QColor glow = Obsidium::ACCENT_COLOR;
        glow.setAlphaF(float(0.08 + 0.42 * m_pulseLevel));
        p.fillPath(path, glow);
        QColor edge = Obsidium::ACCENT_COLOR;
        edge.setAlphaF(float(0.25 + 0.75 * m_pulseLevel));
        p.setPen(QPen(edge, qMax<qreal>(1.0, 2 * m_scale)));
        p.drawPath(path);
    }

    if (m_style == Style::Bare || text().isEmpty())
        return;

    // keep the caption off the gems at the ends of the painted button
    const qreal inset = (m_style == Style::Play ? 30 : m_style == Style::Tab ? 26 : 22) * m_scale;
    const QRectF textArea = r.adjusted(inset, 0, -inset, 0);
    QFont font = Obsidium::pixelFont(m_basePx * m_scale);
    // a long caption gets smaller rather than running over the gems
    for (int i = 0; i < 8 && QFontMetricsF(font).horizontalAdvance(text()) > textArea.width(); i++)
        font.setPixelSize(qMax(1, font.pixelSize() - 1));
    p.setFont(font);
    const QPointF offset = down ? QPointF(0, 1.5 * m_scale) : QPointF();
    const QRectF textRect = textArea.translated(offset);
    // a hard pixel shadow, as in the game
    const qreal shadow = qMax<qreal>(1.0, 2 * m_scale);
    p.setPen(QColor(20, 12, 24, isEnabled() ? 220 : 120));
    p.drawText(textRect.translated(shadow, shadow), Qt::AlignCenter, text());

    QColor color = Obsidium::TEXT_COLOR;
    if (!isEnabled())
        color = QColor(130, 122, 118);
    else if (m_style == Style::Play || active)
        color = QColor(255, 255, 255);
    else if (m_hover)
        color = QColor(255, 244, 226);
    p.setPen(color);
    p.drawText(textRect, Qt::AlignCenter, text());
}

// ---- dimming --------------------------------------------------------------------------------

namespace {

/// The picture's own shape in black, laid over it: only the picture gets darker, not the
/// transparent corners around it.
class DimOverlay : public QWidget {
   public:
    explicit DimOverlay(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        m_fade = new QVariantAnimation(this);
        m_fade->setDuration(160);
        connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_level = value.toReal();
            setVisible(m_level > 0.001);
            update();
        });
        hide();
    }

    void setShape(const QImage& picture)
    {
        m_shape = QImage(picture.size(), QImage::Format_ARGB32_Premultiplied);
        m_shape.fill(Qt::transparent);
        QPainter p(&m_shape);
        p.drawImage(0, 0, picture);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(m_shape.rect(), Qt::black);
        p.end();
        update();
    }

    void fadeTo(qreal level)
    {
        m_fade->stop();
        m_fade->setStartValue(m_level);
        m_fade->setEndValue(level);
        if (level > 0)
            raise();
        m_fade->start();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_shape.isNull())
            return;
        QPainter p(this);
        p.setOpacity(m_level);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(rect(), m_shape);
    }

   private:
    QImage m_shape;
    qreal m_level = 0;
    QVariantAnimation* m_fade;
};

/// Darkens a whole window (a dialog under another dialog).
class WindowDim : public QWidget {
   public:
    explicit WindowDim(QWidget* window) : QWidget(window)
    {
        setObjectName(QStringLiteral("obsidiumWindowDim"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        // the launcher's own windows are rounded (see ObsidiumDialog::paintEvent)
        m_rounded = window->inherits("ObsidiumDialog") || window->property("obsidiumRounded").toBool();
        m_fade = new QVariantAnimation(this);
        m_fade->setDuration(160);
        connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_level = value.toReal();
            setVisible(m_level > 0.001);
            update();
        });
        window->installEventFilter(this);
        setGeometry(window->rect());
        hide();
    }

    static WindowDim* of(QWidget* window)
    {
        // (findChild needs a Q_OBJECT class in Qt 6.10: looked up as a QWidget by its name)
        if (auto* dim = window->findChild<QWidget*>(QStringLiteral("obsidiumWindowDim"), Qt::FindDirectChildrenOnly))
            return static_cast<WindowDim*>(dim);
        return new WindowDim(window);
    }

    void fadeTo(qreal level)
    {
        if (qFuzzyCompare(m_fade->endValue().toReal() + 1, level + 1) && m_fade->state() == QAbstractAnimation::Running)
            return;
        m_fade->stop();
        m_fade->setStartValue(m_level);
        m_fade->setEndValue(level);
        if (level > 0) {
            setGeometry(parentWidget()->rect());
            raise();
        }
        m_fade->start();
    }

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QColor shade(0, 0, 0);
        shade.setAlphaF(float(m_level));
        if (m_rounded) {
            QPainterPath path;
            path.addRoundedRect(QRectF(rect()).adjusted(2, 2, -2, -2), 10, 10);
            p.fillPath(path, shade);
        } else {
            p.fillRect(rect(), shade);
        }
    }

   private:
    bool m_rounded = false;
    qreal m_level = 0;
    QVariantAnimation* m_fade;
};

/// Watches the whole application for dialogs being shown and hidden: everything under the
/// newest one gets darker - the main window and the dialogs opened before.
class DialogWatcher : public QObject {
   public:
    DialogWatcher(ObsidiumFrame* frame, QWidget* mainWindow) : QObject(frame), m_frame(frame), m_main(mainWindow) {}

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const auto type = event->type();
        if ((type != QEvent::Show && type != QEvent::Hide) || !watched->isWidgetType())
            return false;
        auto* widget = static_cast<QWidget*>(watched);
        if (!widget->isWindow() || widget == m_main || (widget->windowType() != Qt::Dialog && widget->windowType() != Qt::Sheet))
            return false;
        if (type == QEvent::Show) {
            if (!m_open.contains(widget)) {
                m_open.append(widget);
                connect(widget, &QObject::destroyed, this, [this, widget] {
                    m_open.removeAll(widget);
                    refresh();
                });
            }
        } else {
            m_open.removeAll(widget);
            if (auto* dim = widget->findChild<QWidget*>(QStringLiteral("obsidiumWindowDim"), Qt::FindDirectChildrenOnly))
                static_cast<WindowDim*>(dim)->fadeTo(0.0);
        }
        refresh();
        return false;
    }

   private:
    void refresh()
    {
        m_frame->setDimmed(!m_open.isEmpty());
        // the newest window stays bright, the ones it covers get darker
        for (int i = 0; i < m_open.size(); i++)
            WindowDim::of(m_open[i])->fadeTo(i + 1 < m_open.size() ? 0.45 : 0.0);
    }

    ObsidiumFrame* m_frame;
    QWidget* m_main;
    QList<QWidget*> m_open;  // in the order they were opened
};

}  // namespace

// ---- ObsidiumFrame ------------------------------------------------------------------------

ObsidiumFrame::ObsidiumFrame(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setTheme(Obsidium::themes().first());
}

void ObsidiumFrame::setTheme(const Obsidium::Theme& newTheme)
{
    m_theme = newTheme;
    m_picture = QImage(newTheme.picture);
    if (m_dim)
        static_cast<DimOverlay*>(m_dim)->setShape(m_picture);
    relayout();
    update();
}

QSize ObsidiumFrame::sizeForScale(qreal scale) const
{
    const qreal w = Obsidium::REFERENCE_WIDTH * scale;
    return { qRound(w), qRound(w * m_theme.base.height() / m_theme.base.width()) };
}

qreal ObsidiumFrame::fittingScale(const QWidget* widget, qreal preferred) const
{
    const QSize full = sizeForScale(1.0);
    QScreen* screen = widget && widget->screen() ? widget->screen() : QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(QPoint(), full);
    const qreal fitW = available.width() * 0.96 / full.width();
    const qreal fitH = available.height() * 0.96 / full.height();
    // A sharper picture may be shown larger than the reference size, a blurrier one may not.
    const qreal sharpness = m_picture.isNull() ? 1.0 : qMax<qreal>(1.0, qreal(m_picture.width()) / full.width());
    return qMin(qMin(qMin(fitW, fitH), sharpness), qMax<qreal>(0.2, preferred));
}

void ObsidiumFrame::place(QWidget* widget, Obsidium::Slot slot)
{
    widget->setParent(this);
    m_placed.append({ widget, slot });
    relayout();
    widget->show();
}

ObsidiumButton* ObsidiumFrame::addButton(const QString& text, ObsidiumButton::Style style, Obsidium::Slot slot, qreal basePixelSize)
{
    auto* button = new ObsidiumButton(text, style, this);
    button->setBasePixelSize(basePixelSize);
    button->setScale(m_scale);
    connect(this, &ObsidiumFrame::scaleChanged, button, &ObsidiumButton::setScale);
    place(button, slot);
    return button;
}

void ObsidiumFrame::setDimmed(bool dimmed)
{
    if (!m_dim) {
        auto* overlay = new DimOverlay(this);
        overlay->setShape(m_picture);
        overlay->setGeometry(rect());
        m_dim = overlay;
    }
    static_cast<DimOverlay*>(m_dim)->fadeTo(dimmed ? 0.45 : 0.0);
}

void ObsidiumFrame::dimForDialogs(QWidget* mainWindow)
{
    qApp->installEventFilter(new DialogWatcher(this, mainWindow));
}

void ObsidiumFrame::relayout()
{
    if (m_dim)
        m_dim->setGeometry(rect());
    // the rectangles are in the picture's coordinates
    const qreal k = m_theme.base.width() > 0 ? qreal(width()) / m_theme.base.width() : 1.0;
    for (const auto& placed : m_placed) {
        if (!placed.widget)
            continue;
        const QRect r = m_theme.rect(placed.slot);
        placed.widget->setGeometry(qRound(r.x() * k), qRound(r.y() * k), qRound(r.width() * k), qRound(r.height() * k));
    }
}

void ObsidiumFrame::resizeEvent(QResizeEvent* event)
{
    const qreal scale = qreal(width()) / Obsidium::REFERENCE_WIDTH;
    if (!qFuzzyCompare(scale, m_scale)) {
        m_scale = scale;
        emit scaleChanged(m_scale);
    }
    relayout();
    QWidget::resizeEvent(event);
}

void ObsidiumFrame::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(rect(), m_picture);
}

void ObsidiumFrame::mousePressEvent(QMouseEvent* event)
{
    // The picture has no title bar: any free spot of it moves the window.
    if (event->button() == Qt::LeftButton) {
        if (auto* handle = window()->windowHandle()) {
            handle->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
