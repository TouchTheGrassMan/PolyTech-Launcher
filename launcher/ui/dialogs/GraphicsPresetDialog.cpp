#include "GraphicsPresetDialog.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/obsidium/ObsidiumFrame.h"

using namespace ObsidiumStyle;

namespace {

// Card layout, in pixels.
constexpr int CARD_W = 320;
constexpr int CARD_GAP = 16;
constexpr int CONTENT_W = CARD_W * 3 + CARD_GAP * 2;
constexpr int ICON = 64;  // the 16x16 pixel art, 4x
constexpr int PAD = 12;
constexpr int TITLE_PX = 16;
constexpr int DESC_PX = 12;
constexpr int DESC_LINE = 16;

const QColor CARD_SELECTED_BG(40, 22, 66);
const QColor CARD_HOVER_BG(20, 16, 38);
const QColor DESC_COLOR(196, 186, 204);
const QColor SEPARATOR(150, 98, 58, 130);

/** Word wrap by words, at most maxWidth per line. */
QStringList wrap(const QString& text, const QFontMetrics& fm, int maxWidth)
{
    QStringList lines;
    QString line;
    for (const QString& word : text.split(' ', Qt::SkipEmptyParts)) {
        const QString candidate = line.isEmpty() ? word : line + ' ' + word;
        if (!line.isEmpty() && fm.horizontalAdvance(candidate) > maxWidth) {
            lines.append(line);
            line = word;
        } else {
            line = candidate;
        }
    }
    if (!line.isEmpty())
        lines.append(line);
    return lines;
}

/** A preset card: the icon, the name and what it does, as in the mod; the launcher's frame. */
class PresetCard : public QAbstractButton {
   public:
    PresetCard(const GraphicsPresetChoice& choice, bool recommended, QWidget* parent)
        : QAbstractButton(parent), m_choice(choice), m_recommended(recommended)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::NoFocus);
        setToolTip(choice.description);
        m_lines = wrap(choice.description, QFontMetrics(Obsidium::pixelFont(DESC_PX)), CARD_W - 2 * PAD - 8);
    }

    QString id() const { return m_choice.id; }
    int lineCount() const { return static_cast<int>(m_lines.size()); }
    /// All cards are as tall as the one with the longest text.
    void setLines(int lines) { setFixedSize(CARD_W, PAD + ICON + 10 + TITLE_PX + 4 + 10 + lines * DESC_LINE + PAD + 4); }

   protected:
    void enterEvent(QEnterEvent* event) override
    {
        update();
        QAbstractButton::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override
    {
        update();
        QAbstractButton::leaveEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool selected = isChecked();
        const bool hovered = underMouse();
        const int w = width();

        const QRectF frame = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        QPainterPath path;
        path.addRoundedRect(frame, 8, 8);
        p.fillPath(path, selected ? CARD_SELECTED_BG : hovered ? CARD_HOVER_BG : PANEL_BG);
        if (selected) {
            p.setPen(QPen(Obsidium::ACCENT_COLOR, 3));
        } else if (hovered) {
            QColor hover = Obsidium::ACCENT_COLOR;
            hover.setAlpha(150);
            p.setPen(QPen(hover, 2));
        } else {
            p.setPen(QPen(BRONZE, 2));
        }
        p.drawPath(path);

        if (m_recommended) {
            const QFont badgeFont = Obsidium::pixelFont(SMALL_PX);
            const QString badge = GraphicsPresetDialog::tr("рекомендуем");
            const int bw = QFontMetrics(badgeFont).horizontalAdvance(badge) + 12;
            const QRectF pill(w - bw - 8, 8, bw, 18);
            QPainterPath pillPath;
            pillPath.addRoundedRect(pill, 4, 4);
            p.fillPath(pillPath, QColor(214, 120, 255, selected ? 90 : 45));
            p.setPen(QPen(Obsidium::ACCENT_COLOR, 1.5));
            p.drawPath(pillPath);
            p.setFont(badgeFont);
            p.setPen(QColor(255, 255, 255));
            p.drawText(pill, Qt::AlignCenter, badge);
        }

        // 16x16 pixel art scaled up without smoothing, like the in-game blit.
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        if (!m_choice.icon.isNull())
            p.drawImage(QRect((w - ICON) / 2, PAD, ICON, ICON), m_choice.icon);

        int y = PAD + ICON + 10;
        p.setFont(Obsidium::pixelFont(TITLE_PX));
        const QRect titleRect(0, y, w, TITLE_PX + 4);
        p.setPen(QColor(0, 0, 0, 170));
        p.drawText(titleRect.translated(1, 1), Qt::AlignHCenter | Qt::AlignTop, m_choice.title);
        p.setPen(selected ? QColor(255, 255, 255) : Obsidium::TEXT_COLOR);
        p.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop, m_choice.title);

        y += TITLE_PX + 4 + 4;
        p.fillRect(QRect(PAD + 8, y, w - 2 * (PAD + 8), 2), selected ? QColor(214, 120, 255, 120) : SEPARATOR);

        y += 10;
        p.setFont(Obsidium::pixelFont(DESC_PX));
        p.setPen(selected ? Obsidium::TEXT_COLOR : DESC_COLOR);
        for (const QString& line : m_lines) {
            p.drawText(QRect(0, y, w, DESC_LINE), Qt::AlignHCenter | Qt::AlignVCenter, line);
            y += DESC_LINE;
        }
    }

   private:
    GraphicsPresetChoice m_choice;
    bool m_recommended;
    QStringList m_lines;
};

QLabel* textLabel(const QString& text, const QString& css, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    label->setFixedWidth(CONTENT_W);
    label->setStyleSheet(css);
    return label;
}

/// Rich text with some air between the lines (the pixel font has none of its own).
QString paragraph(const QString& html)
{
    return QStringLiteral("<p style=\"line-height: 125%; margin: 0;\">%1</p>").arg(html);
}

QString gigabytes(uint64_t mib)
{
    return QString::number(static_cast<double>(mib) / 1024.0, 'f', 1).replace('.', ',');
}

/// "Intel Core i5-8250U, Intel UHD Graphics 620, 8,0 ГБ ОЗУ (свободно 3,1 ГБ)"
QString configuration(const HardwareRating::Assessment& hw)
{
    QStringList parts;
    parts << (hw.cpu.name.isEmpty() ? GraphicsPresetDialog::tr("процессор не определён") : hw.cpu.name);
    parts << (hw.gpu.name.isEmpty() ? GraphicsPresetDialog::tr("видеокарта не определена") : hw.gpu.name);
    if (hw.totalRamMiB > 0) {
        QString ram = GraphicsPresetDialog::tr("%1 ГБ ОЗУ").arg(gigabytes(hw.totalRamMiB));
        if (hw.availableRamMiB > 0)
            ram += GraphicsPresetDialog::tr(" (свободно %1 ГБ)").arg(gigabytes(hw.availableRamMiB));
        parts << ram;
    }
    return parts.join(QStringLiteral(", "));
}

/// What exactly is below the minimum, for the line under the warning.
QString shortfall(const HardwareRating::Assessment& hw)
{
    QStringList parts;
    if (hw.cpuBelowMinimum)
        parts << GraphicsPresetDialog::tr("процессор");
    if (hw.gpuBelowMinimum)
        parts << GraphicsPresetDialog::tr("видеокарта");
    if (hw.ramBelowMinimum)
        parts << GraphicsPresetDialog::tr("свободная оперативная память (сейчас %1 МБ, нужно %2 МБ — закройте лишние программы)")
                     .arg(hw.availableRamMiB)
                     .arg(HardwareRating::MIN_FREE_RAM_MIB);
    return GraphicsPresetDialog::tr("Ниже минимума: %1.").arg(parts.join(QStringLiteral(", ")));
}

}  // namespace

GraphicsPresetDialog::GraphicsPresetDialog(const QVector<GraphicsPresetChoice>& choices,
                                           const GraphicsPromptTexts& texts,
                                           const HardwareRating::Assessment& hardware,
                                           QWidget* parent)
    : ObsidiumDialog(parent), m_texts(texts)
{
    setTitle(tr("Пресет графики"));
    setTitleCentered(true);
    body()->setContentsMargins(22, 12, 22, 20);
    body()->setSpacing(0);
    body()->addSpacing(6);

    body()->addWidget(textLabel(texts.header, labelCss(Obsidium::TEXT_COLOR, 14), this), 0, Qt::AlignHCenter);
    body()->addSpacing(14);

    // the suggested preset, if the pack has it; the button needs a choice otherwise
    QString recommendedTitle;
    for (const auto& choice : choices) {
        if (choice.id == hardware.recommended) {
            m_selected = choice.id;
            recommendedTitle = choice.title;
        }
    }

    auto* cards = new QHBoxLayout();
    cards->setSpacing(CARD_GAP);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    QList<PresetCard*> cardList;
    int lines = 0;
    for (const auto& choice : choices) {
        auto* card = new PresetCard(choice, choice.id == m_selected, this);
        card->setChecked(choice.id == m_selected);
        group->addButton(card);
        cards->addWidget(card);
        cardList << card;
        lines = std::max(lines, card->lineCount());
        connect(card, &QAbstractButton::clicked, this, [this, card] {
            m_selected = card->id();
            m_confirm->setEnabled(true);
            m_confirm->setText(m_texts.confirm);
        });
    }
    for (auto* card : cardList)
        card->setLines(lines);
    body()->addLayout(cards);
    body()->addSpacing(14);

    // what this computer is and what suits it
    QString about = tr("Ваша конфигурация: %1.").arg(configuration(hardware).toHtmlEscaped());
    if (!recommendedTitle.isEmpty())
        about += tr(" С учетом вашего устройства рекомендуем вам настройки <span style=\"color: %1;\">«%2»</span>.")
                     .arg(rgb(Obsidium::ACCENT_COLOR), recommendedTitle.toHtmlEscaped());
    auto* info = textLabel(paragraph(about), hintCss(), this);
    info->setTextFormat(Qt::RichText);
    info->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    body()->addWidget(info, 0, Qt::AlignHCenter);

    if (!hardware.meetsMinimum()) {
        body()->addSpacing(10);
        const QString warning =
            tr("Ваша конфигурация устройства не соответствует минимальным системным требованиям для комфортной игры. "
               "Вероятно, вы столкнетесь с серьезными проблемами с производительностью");
        const QString html = QStringLiteral("%1<br><span style=\"color: %2; font-size: %3px;\">%4</span>")
                                 .arg(warning.toHtmlEscaped(), rgb(MUTED))
                                 .arg(SMALL_PX + 1)
                                 .arg(shortfall(hardware).toHtmlEscaped());
        auto* alert = textLabel(paragraph(html),
                                QStringLiteral("QLabel { %1 color: %2; background: rgba(255, 184, 77, 28); "
                                               "border: 2px solid %2; border-radius: 6px; padding: 7px 10px; }")
                                    .arg(fontCss(TEXT_PX), rgb(WARNING)),
                                this);
        alert->setTextFormat(Qt::RichText);
        alert->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        body()->addWidget(alert, 0, Qt::AlignHCenter);
    }
    body()->addSpacing(16);

    m_confirm = new QPushButton(m_selected.isEmpty() ? texts.selectFirst : texts.confirm, this);
    m_confirm->setStyleSheet(accentButtonCss() + QStringLiteral("QPushButton:disabled { color: #b9a6c8; background: #2a1d40; "
                                                                "border-color: #5a4470; }"));
    m_confirm->setCursor(Qt::PointingHandCursor);
    m_confirm->setMinimumWidth(320);
    m_confirm->setEnabled(!m_selected.isEmpty());
    m_confirm->setDefault(true);
    m_confirm->setAutoDefault(true);
    connect(m_confirm, &QPushButton::clicked, this, &QDialog::accept);
    body()->addWidget(m_confirm, 0, Qt::AlignHCenter);
    body()->addSpacing(12);

    body()->addWidget(textLabel(texts.footnote, labelCss(MUTED, SMALL_PX + 1), this), 0, Qt::AlignHCenter);

    adjustSize();
    setFixedSize(sizeHint());
    m_confirm->setFocus();
}
