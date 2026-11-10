// SPDX-License-Identifier: GPL-3.0-only
#include "DeveloperPage.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include "ui/obsidium/ObsidiumFrame.h"

DeveloperPage::DeveloperPage(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void DeveloperPage::setPreview(const QImage& preview)
{
    m_preview = preview;
    update();
}

void DeveloperPage::setTitle(const QString& title)
{
    m_title = title;
    update();
}

void DeveloperPage::setDescription(const QString& description)
{
    m_description = description;
    update();
}

void DeveloperPage::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF area = QRectF(rect()).adjusted(2, 2, -2, -2);
    const qreal radius = qMax<qreal>(4.0, height() * 0.015);
    QPainterPath clip;
    clip.addRoundedRect(area, radius, radius);
    p.setClipPath(clip);

    // the picture fills the panel, cropped rather than stretched
    if (!m_preview.isNull()) {
        const QSizeF size = QSizeF(m_preview.size()).scaled(area.size(), Qt::KeepAspectRatioByExpanding);
        const QRectF target(area.center().x() - size.width() / 2, area.center().y() - size.height() / 2, size.width(), size.height());
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(target, m_preview);
    } else {
        p.fillRect(area, QColor(5, 8, 20));
    }

    // darker at the top (title) and at the bottom (description)
    QLinearGradient shade(area.topLeft(), area.bottomLeft());
    shade.setColorAt(0.0, QColor(5, 8, 20, 200));
    shade.setColorAt(0.22, QColor(5, 8, 20, 0));
    shade.setColorAt(0.55, QColor(5, 8, 20, 0));
    shade.setColorAt(1.0, QColor(5, 8, 20, 235));
    p.fillRect(area, shade);

    const qreal margin = height() * 0.045;
    const QFont titleFont = Obsidium::pixelFont(height() * 0.07);
    const QFont textFont = Obsidium::pixelFont(height() * 0.042);
    const qreal textWidth = area.width() * 0.66;
    const QRectF titleRect(area.left() + margin, area.top() + margin * 0.8, area.width() - 2 * margin, QFontMetricsF(titleFont).height());
    const QRectF textBounds = QFontMetricsF(textFont).boundingRect(QRectF(0, 0, textWidth, area.height()), Qt::TextWordWrap, m_description);
    const QRectF textRect(area.left() + margin, area.bottom() - margin - textBounds.height(), textWidth, textBounds.height());

    const qreal shadow = qMax<qreal>(1.0, height() * 0.004);
    auto drawText = [&](const QRectF& r, const QFont& font, const QString& text, const QColor& color, int flags) {
        p.setFont(font);
        p.setPen(QColor(10, 6, 16, 220));
        p.drawText(r.translated(shadow, shadow), flags, text);
        p.setPen(color);
        p.drawText(r, flags, text);
    };
    drawText(titleRect, titleFont, m_title, QColor(255, 255, 255), Qt::AlignLeft | Qt::AlignTop);
    drawText(textRect, textFont, m_description, Obsidium::TEXT_COLOR, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
}
