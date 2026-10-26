#include "GraphicsPresetDialog.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// Minecraft GUI pixels -> screen pixels (GUI scale 2).
constexpr int S = 2;

// Colours of GraphicsPresetPrompt (ARGB).
const QColor PANEL_BG = QColor::fromRgba(0xC8000000);
const QColor PANEL_BORDER = QColor::fromRgba(0xFF5A5A5A);
const QColor CARD_BG = QColor::fromRgba(0x66000000);
const QColor CARD_BG_HOVER = QColor::fromRgba(0x99000000);
const QColor CARD_BORDER = QColor::fromRgba(0xFF6E6E6E);
const QColor CARD_BORDER_HOVER = QColor::fromRgba(0xFFA0A0A0);
const QColor CARD_BORDER_SELECTED = QColor::fromRgba(0xFFFFFFFF);
const QColor SEPARATOR = QColor::fromRgba(0x40FFFFFF);
const QColor TEXT = QColor::fromRgba(0xFFFFFFFF);
const QColor TEXT_TITLE = QColor::fromRgba(0xFFDDDDDD);
const QColor TEXT_DIM = QColor::fromRgba(0xFFA8A8A8);
const QColor TEXT_FAINT = QColor::fromRgba(0xFF8A8A8A);
// The mod draws over the dimmed menu panorama; here the window itself is dark.
const QColor WINDOW_BG = QColor(0x14, 0x14, 0x14);

// Layout of GraphicsPresetPrompt, in GUI pixels.
constexpr int ICON_SIZE = 32;
constexpr int DESC_LINES = 4;
constexpr int GAP = 8;
constexpr int CARD_W = 150;
constexpr int CARD_H = 6 + ICON_SIZE + 4 + 10 + 4 + DESC_LINES * 10 + 4;
constexpr int PANEL_PAD = 10;
constexpr int CONTENT_W = CARD_W * 3 + GAP * 2;

// Minecraft's font: 8 px glyphs on a 10 px line.
QFont mcFont()
{
    QFont font;
    font.setPixelSize(8 * S - 1);
    return font;
}

/** Minecraft's drop shadow: the text colour at a quarter brightness, offset by one GUI pixel. */
QColor shadowOf(const QColor& c)
{
    return QColor((c.red() & 0xFC) >> 2, (c.green() & 0xFC) >> 2, (c.blue() & 0xFC) >> 2);
}

void outline(QPainter& p, const QRect& r, const QColor& c)
{
    p.fillRect(r.x(), r.y(), r.width(), S, c);
    p.fillRect(r.x(), r.bottom() - S + 1, r.width(), S, c);
    p.fillRect(r.x(), r.y(), S, r.height(), c);
    p.fillRect(r.right() - S + 1, r.y(), S, r.height(), c);
}

/** Word wrap like Font.split: by words, at most maxWidth per line. */
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

QLabel* textLabel(const QString& text, const QColor& color, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setFont(mcFont());
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    label->setFixedWidth((CONTENT_W - 8) * S);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, color);
    label->setPalette(pal);
    return label;
}

/** A preset card, painted like GraphicsPresetPrompt.CardWidget. */
class PresetCard : public QAbstractButton {
   public:
    PresetCard(const GraphicsPresetChoice& choice, QWidget* parent) : QAbstractButton(parent), m_choice(choice)
    {
        setCheckable(true);
        setFixedSize(CARD_W * S, CARD_H * S);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setFont(mcFont());
        setToolTip(choice.description);
        m_lines = wrap(choice.description, QFontMetrics(font()), (CARD_W - 12) * S);
    }

    QString id() const { return m_choice.id; }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const bool selected = isChecked();
        const bool hovered = underMouse();
        const int w = width();

        p.fillRect(rect(), hovered ? CARD_BG_HOVER : CARD_BG);
        const QColor border = selected ? CARD_BORDER_SELECTED : hovered ? CARD_BORDER_HOVER : CARD_BORDER;
        outline(p, rect(), border);
        if (selected)
            outline(p, rect().adjusted(S, S, -S, -S), border);

        // 16x16 pixel art scaled up without smoothing, like the in-game blit.
        if (!m_choice.icon.isNull())
            p.drawImage(QRect((w - ICON_SIZE * S) / 2, 6 * S, ICON_SIZE * S, ICON_SIZE * S), m_choice.icon);

        int y = (6 + ICON_SIZE + 4) * S;
        const QColor titleColor = selected ? TEXT : TEXT_TITLE;
        p.setPen(shadowOf(titleColor));
        p.drawText(QRect(S, y + S, w, 9 * S), Qt::AlignHCenter | Qt::AlignTop, m_choice.title);
        p.setPen(titleColor);
        p.drawText(QRect(0, y, w, 9 * S), Qt::AlignHCenter | Qt::AlignTop, m_choice.title);

        y += 11 * S;
        p.fillRect(6 * S, y, w - 12 * S, S, SEPARATOR);

        y += 4 * S;
        p.setPen(TEXT_DIM);
        for (int i = 0; i < m_lines.size() && i < DESC_LINES; ++i) {
            p.drawText(QRect(0, y, w, 10 * S), Qt::AlignHCenter | Qt::AlignTop, m_lines[i]);
            y += 10 * S;
        }
    }

   private:
    GraphicsPresetChoice m_choice;
    QStringList m_lines;
};

// Vanilla 1.20.1 button: grey, blue-tinted on hover, dark when inactive.
const char* BUTTON_STYLE =
    "QPushButton { background: #6f6f6f; color: #ffffff; border: 2px solid #000000; padding: 0; }"
    "QPushButton:hover { background: #7e88bf; }"
    "QPushButton:disabled { background: #2c2c2c; color: #a0a0a0; }";

}  // namespace

GraphicsPresetDialog::GraphicsPresetDialog(const QVector<GraphicsPresetChoice>& choices, const GraphicsPromptTexts& texts, QWidget* parent)
    : QDialog(parent), m_texts(texts)
{
    setWindowTitle(tr("Пресет графики"));
    setModal(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, WINDOW_BG);
    setPalette(pal);
    setAutoFillBackground(true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);

    // The panel: translucent black with a grey outline.
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("panel"));
    panel->setStyleSheet(QStringLiteral("QFrame#panel { background: rgba(0,0,0,%1); border: %2px solid %3; }")
                             .arg(PANEL_BG.alpha())
                             .arg(S)
                             .arg(PANEL_BORDER.name()));
    outer->addWidget(panel);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(PANEL_PAD * S, 8 * S, PANEL_PAD * S, 4 * S);
    layout->setSpacing(0);

    layout->addWidget(textLabel(texts.header, TEXT, panel), 0, Qt::AlignHCenter);
    layout->addSpacing(8 * S);

    auto* cards = new QHBoxLayout();
    cards->setSpacing(GAP * S);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    for (const auto& choice : choices) {
        auto* card = new PresetCard(choice, panel);
        group->addButton(card);
        cards->addWidget(card);
        connect(card, &QAbstractButton::clicked, this, [this, card] {
            m_selected = card->id();
            m_confirm->setEnabled(true);
            m_confirm->setText(m_texts.confirm);
        });
    }
    layout->addLayout(cards);
    layout->addSpacing(8 * S);

    m_confirm = new QPushButton(texts.selectFirst, panel);
    m_confirm->setStyleSheet(BUTTON_STYLE);
    m_confirm->setFont(mcFont());
    m_confirm->setFixedSize(qMin(200, CONTENT_W) * S, 20 * S);
    m_confirm->setEnabled(false);
    m_confirm->setDefault(true);
    connect(m_confirm, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(m_confirm, 0, Qt::AlignHCenter);
    layout->addSpacing(6 * S);

    layout->addWidget(textLabel(texts.footnote, TEXT_FAINT, panel), 0, Qt::AlignHCenter);

    setFixedSize(sizeHint());
}
