// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonVersionsWidget.h"

#include <algorithm>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"

namespace {
QNetworkRequest request(const QString& url)
{
    QNetworkRequest out{ QUrl(url) };
    out.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
    if (url.startsWith("https://api.github.com/"))
        out.setRawHeader("Accept", "application/vnd.github+json");
    out.setTransferTimeout(30000);
    return out;
}
}  // namespace

HorizonVersionsWidget::HorizonVersionsWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* top = new QHBoxLayout();
    m_list = new QTreeWidget(this);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAllColumnsShowFocus(true);
    m_list->setHeaderLabels({ tr("Версия"), tr("Сборка Horizon"), tr("Выпущено"), tr("Тип") });
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; column++)
        m_list->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    top->addWidget(m_list, 1);

    auto* side = new QVBoxLayout();
    m_showLabel = new QLabel(tr("Показывать"), this);
    m_showLabel->setAlignment(Qt::AlignHCenter);
    side->addWidget(m_showLabel);
    m_nightly = new QCheckBox(tr("Nightly"), this);
    m_nightly->setToolTip(tr("Ежедневные сборки Horizon: самые свежие исправления, но и самые свежие ошибки."));
    side->addWidget(m_nightly);
    side->addSpacing(12);
    m_autoUpdate = new QCheckBox(tr("Обновлять\nавтоматически"), this);
    m_autoUpdate->setChecked(true);
    m_autoUpdate->setToolTip(tr("При каждом запуске экземпляр проверяет GitHub и переходит на новую сборку Horizon "
                                "для той же версии Minecraft."));
    side->addWidget(m_autoUpdate);
    side->addStretch(1);
    m_refresh = new QPushButton(tr("Обновить"), this);
    side->addWidget(m_refresh);
    top->addLayout(side);
    layout->addLayout(top, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_about = new QLabel(this);
    m_about->setWordWrap(true);
    m_about->setTextFormat(Qt::RichText);
    m_about->setOpenExternalLinks(true);
    m_about->setText(
        tr("<b>Horizon</b> — неофициальная версия Minecraft: Java Edition, переписанная на Rust "
           "(<a href=\"https://github.com/horizon-rust/horizon\">github.com/horizon-rust/horizon</a>). "
           "Нужна лицензионная учётная запись Microsoft с Minecraft: Java Edition. Моды Forge, Fabric и NeoForge не запускаются.<br>"
           "При первом запуске лаунчер скачает Horizon и файлы Minecraft и подготовит из них ресурсы игры — это займёт несколько минут."));
    layout->addWidget(m_about);

    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &HorizonVersionsWidget::selectionChanged);
    connect(m_nightly, &QCheckBox::toggled, this, &HorizonVersionsWidget::fillList);
    connect(m_autoUpdate, &QCheckBox::toggled, this, &HorizonVersionsWidget::selectionChanged);
    connect(m_refresh, &QPushButton::clicked, this, &HorizonVersionsWidget::refresh);
}

void HorizonVersionsWidget::ensureLoaded()
{
    if (m_loaded)
        return;
    m_loaded = true;
    refresh();
}

void HorizonVersionsWidget::setStatus(const QString& text)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

void HorizonVersionsWidget::refresh()
{
    if (m_reply)
        m_reply->abort();
    m_rows.clear();
    m_list->clear();
    emit selectionChanged();
    if (Horizon::platformAsset().isEmpty()) {
        setStatus(tr("Horizon не выпускается для этой системы."));
        return;
    }
    setStatus(tr("Загрузка списка сборок Horizon..."));
    m_refresh->setEnabled(false);
    auto* reply = APPLICATION->network()->get(request(Horizon::releasesUrl()));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_refresh->setEnabled(true);
            if (reply->error() != QNetworkReply::OperationCanceledError)
                setStatus(tr("Не удалось получить список сборок Horizon: %1").arg(reply->errorString()));
            return;
        }
        onReleases(reply->readAll());
    });
}

void HorizonVersionsWidget::onReleases(const QByteArray& json)
{
    // The newest release and the newest Nightly; each says which Minecraft it plays.
    const auto releases = Horizon::parseReleases(json);
    for (const auto& channel : { QStringLiteral("release"), QStringLiteral("nightly") }) {
        const auto release = Horizon::newest(releases, channel);
        if (!release.isValid())
            continue;
        bool known = false;
        for (const auto& row : m_rows)
            known = known || row.release.key() == release.key();
        if (!known)
            m_rows.append({ release, QString() });
    }
    if (m_rows.isEmpty()) {
        m_refresh->setEnabled(true);
        setStatus(tr("На GitHub пока нет сборок Horizon для этой системы."));
        return;
    }
    loadPins(0);
}

void HorizonVersionsWidget::loadPins(int index)
{
    if (index >= m_rows.size()) {
        m_refresh->setEnabled(true);
        setStatus(QString());
        fillList();
        return;
    }
    auto* reply = APPLICATION->network()->get(request(Horizon::researchLockUrl(m_rows[index].release.tag)));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, index] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        if (reply->error() == QNetworkReply::NoError && index < m_rows.size())
            m_rows[index].minecraft = Horizon::parsePins(reply->readAll()).minecraft;
        probeLaunchApi(index);
    });
}

void HorizonVersionsWidget::probeLaunchApi(int index)
{
    auto* reply = APPLICATION->network()->head(request(Horizon::launchApiProbeUrl(m_rows[index].release.tag)));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, index] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        if (index < m_rows.size())
            m_rows[index].launchApi = reply->error() == QNetworkReply::NoError;
        loadPins(index + 1);
    });
}

void HorizonVersionsWidget::fillList()
{
    m_list->clear();
    bool first = true;
    // Without a stable release that the launcher can start, the Nightly stands in for it, so it is always shown then.
    const bool onlyNightly = std::none_of(m_rows.begin(), m_rows.end(), [](const Row& r) {
        return r.release.channel != "nightly" && r.launchApi && !r.minecraft.isEmpty();
    });
    // Nothing to choose between then.
    m_showLabel->setVisible(!onlyNightly);
    m_nightly->setVisible(!onlyNightly);
    for (int i = 0; i < m_rows.size(); i++) {
        const auto& row = m_rows[i];
        if (row.minecraft.isEmpty() || !row.launchApi)
            continue;  // which Minecraft it plays is unknown, or it can't be started from here (no Launch API)
        const bool nightly = row.release.channel == "nightly";
        if (nightly && !m_nightly->isChecked() && !onlyNightly)
            continue;
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(0, row.minecraft);
        item->setText(1, row.release.displayName());
        item->setText(2, QLocale().toString(row.release.published.toLocalTime().date(), "dd.MM.yyyy"));
        item->setText(3, nightly ? QStringLiteral("nightly") : QStringLiteral("release"));
        item->setData(0, Qt::UserRole, i);
        if (first) {
            item->setIcon(0, QIcon::fromTheme("star"));
            first = false;
        }
    }
    if (m_list->topLevelItemCount() > 0)
        m_list->setCurrentItem(m_list->topLevelItem(0));
    else if (!m_rows.isEmpty())
        setStatus(tr("Не удалось узнать, для какой версии Minecraft сделаны сборки Horizon. Нажмите «Обновить»."));
    emit selectionChanged();
}

bool HorizonVersionsWidget::hasSelection() const
{
    return m_list->currentItem() != nullptr && m_list->currentItem()->isSelected();
}

QString HorizonVersionsWidget::selectedMinecraftVersion() const
{
    if (!hasSelection())
        return {};
    return m_rows.value(m_list->currentItem()->data(0, Qt::UserRole).toInt()).minecraft;
}

Horizon::Release HorizonVersionsWidget::selectedRelease() const
{
    if (!hasSelection())
        return {};
    return m_rows.value(m_list->currentItem()->data(0, Qt::UserRole).toInt()).release;
}

bool HorizonVersionsWidget::autoUpdate() const
{
    return m_autoUpdate->isChecked();
}
