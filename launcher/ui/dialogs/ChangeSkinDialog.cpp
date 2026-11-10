#include "ChangeSkinDialog.h"

#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIODevice>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "BaseInstance.h"
#include "SkinPreview3D.h"
#include "PolytechSkins.h"
#include "ui/obsidium/ObsidiumFrame.h"

// Front-of-head region in a 64x64 (or 64x32) skin, used for list thumbnails.
static QIcon faceIcon(const QString& pngPath)
{
    QImage img(pngPath);
    if (img.isNull())
        return QIcon();
    QPixmap face = ObsidiumStyle::faceOf(img, 32);
    if (face.isNull())
        face = QPixmap::fromImage(img.scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation));
    // the same picture when selected: the style must not tint the face with the highlight
    QIcon icon;
    for (auto mode : { QIcon::Normal, QIcon::Selected, QIcon::Active })
        icon.addPixmap(face, mode);
    return icon;
}

ChangeSkinDialog::ChangeSkinDialog(BaseInstance* instance, QWidget* parent) : ObsidiumDialog(parent), m_instance(instance)
{
    if (m_instance) {
        m_gameRoot = m_instance->gameRoot();
        m_skinDir = PolytechSkins::instanceDir(m_gameRoot);
    }
    setup();
}

ChangeSkinDialog::ChangeSkinDialog(const QString& nick, BaseInstance* instance, QWidget* parent)
    : ObsidiumDialog(parent), m_instance(instance), m_nick(nick)
{
    if (m_instance)
        m_gameRoot = m_instance->gameRoot();
    m_skinDir = PolytechSkins::accountDir(nick);
    setup();
}

void ChangeSkinDialog::setup()
{
    using namespace ObsidiumStyle;
    setTitle(m_nick.isEmpty() ? tr("Скины") : tr("Скин: %1").arg(m_nick));
    setAcceptDrops(true);
    setFixedSize(640, 430);

    m_dataRoot = APPLICATION->dataRoot();

    auto* outer = body();

    // Warning shown when the mod isn't installed in this instance.
    m_modWarning = new QLabel(this);
    m_modWarning->setWordWrap(true);
    m_modWarning->setStyleSheet(labelCss(WARNING, SMALL_PX));
    m_modWarning->setText(tr("Мод polytechskins не установлен в выбранной сборке — там скины не отобразятся, пока его не добавить."));
    m_modWarning->setVisible(false);
    outer->addWidget(m_modWarning);

    auto* main = new QHBoxLayout();
    main->setSpacing(14);
    outer->addLayout(main, 1);

    // ---- Left: the shared library list + its buttons ----
    auto* leftBox = new QVBoxLayout();
    leftBox->setSpacing(7);
    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(20, 20));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setStyleSheet(listCss());
    leftBox->addWidget(m_list, 1);

    auto* dropHint = new QLabel(tr("Файлы .png можно перетащить прямо в это окно."), this);
    dropHint->setWordWrap(true);
    dropHint->setStyleSheet(labelCss(MUTED, SMALL_PX));
    leftBox->addWidget(dropHint);

    auto* listButtons = new QHBoxLayout();
    listButtons->setSpacing(6);
    auto* addBtn = new QPushButton(tr("Добавить"), this);
    m_renameBtn = new QPushButton(tr("Переименовать"), this);
    m_removeBtn = new QPushButton(tr("Удалить"), this);
    for (auto* button : { addBtn, m_renameBtn, m_removeBtn }) {
        // three in a row: narrower padding so that the captions fit
        button->setStyleSheet(buttonCss() + QStringLiteral("QPushButton { padding: 5px 4px; }"));
        button->setCursor(Qt::PointingHandCursor);
        button->setAutoDefault(false);
        button->setMinimumWidth(button->sizeHint().width());  // the caption is never cut
        listButtons->addWidget(button);
    }
    leftBox->addLayout(listButtons);
    main->addLayout(leftBox, 1);

    // ---- Right: preview + model + apply ----
    auto* rightBox = new QVBoxLayout();
    rightBox->setSpacing(7);

    auto* previewFrame = new QWidget(this);
    previewFrame->setObjectName(QStringLiteral("previewFrame"));
    previewFrame->setStyleSheet(panelCss(QStringLiteral("previewFrame")));
    auto* previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(4, 4, 4, 4);
    m_preview = new SkinPreview3D(previewFrame);
    m_preview->setBackground(Qt::transparent);
    previewLayout->addWidget(m_preview);
    rightBox->addWidget(previewFrame, 1);

    auto* modelRow = new QHBoxLayout();
    auto* modelLabel = new QLabel(tr("Модель:"), this);
    modelLabel->setStyleSheet(labelCss(Obsidium::TEXT_COLOR, TEXT_PX));
    modelRow->addWidget(modelLabel);
    m_classic = new QRadioButton(tr("Обычная"), this);
    m_slim = new QRadioButton(tr("Тонкая"), this);
    m_classic->setToolTip(tr("Руки шириной 4 пикселя, как у Стива"));
    m_slim->setToolTip(tr("Руки шириной 3 пикселя, как у Алекс"));
    for (auto* radio : { m_classic, m_slim }) {
        radio->setStyleSheet(radioCss());
        radio->setCursor(Qt::PointingHandCursor);
        modelRow->addWidget(radio);
    }
    modelRow->addStretch(1);
    m_classic->setChecked(true);
    rightBox->addLayout(modelRow);

    m_applyBtn = new QPushButton(this);
    m_applyBtn->setMinimumWidth(1);  // a long nick is shortened instead (updateApplyText)
    m_applyBtn->setStyleSheet(buttonCss());
    m_applyBtn->setCursor(Qt::PointingHandCursor);
    m_applyBtn->setAutoDefault(false);
    rightBox->addWidget(m_applyBtn);

    m_hint = new QLabel(m_nick.isEmpty() ? tr("Библиотека скинов общая для всего лаунчера. Активный отмечен цветом.")
                                         : tr("Библиотека скинов общая для всего лаунчера. Активный (отмечен цветом) будет у ника %1 "
                                              "в любой сборке.")
                                               .arg(m_nick),
                        this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(labelCss(MUTED, SMALL_PX));
    rightBox->addWidget(m_hint);

    main->addLayout(rightBox, 1);

    connect(addBtn, &QPushButton::clicked, this, &ChangeSkinDialog::addSkin);
    connect(m_renameBtn, &QPushButton::clicked, this, &ChangeSkinDialog::renameSkin);
    connect(m_removeBtn, &QPushButton::clicked, this, &ChangeSkinDialog::removeSkin);
    connect(m_applyBtn, &QPushButton::clicked, this, &ChangeSkinDialog::applySkin);
    // a double click on a skin chooses it, like the button
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { applySkin(); });
    connect(m_list, &QListWidget::currentRowChanged, this, &ChangeSkinDialog::onSelectionChanged);
    connect(m_classic, &QRadioButton::toggled, this, &ChangeSkinDialog::onModelToggled);
    connect(m_slim, &QRadioButton::toggled, this, &ChangeSkinDialog::onModelToggled);

    loadLibrary();
    ensureDefaultSkin();
    loadActive();
    checkMod();
    refreshList();
}

// ---------------- paths ----------------

QString ChangeSkinDialog::libDir() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("polytechskins"));
}

QString ChangeSkinDialog::libSkinsDir() const
{
    return QDir(libDir()).filePath(QStringLiteral("skins"));
}

QString ChangeSkinDialog::libPngFor(const QString& id) const
{
    return QDir(libSkinsDir()).filePath(id + QStringLiteral(".png"));
}

int ChangeSkinDialog::currentIndex() const
{
    int row = m_list->currentRow();
    if (row < 0 || row >= m_skins.size())
        return -1;
    return row;
}

// ---------------- global library ----------------

void ChangeSkinDialog::loadLibrary()
{
    m_skins.clear();

    QFile f(QDir(libDir()).filePath(QStringLiteral("library.json")));
    if (!f.open(QIODevice::ReadOnly))
        return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    for (const QJsonValue& v : doc.object().value(QStringLiteral("skins")).toArray()) {
        QJsonObject o = v.toObject();
        SkinEntry e;
        e.id = o.value(QStringLiteral("id")).toString();
        e.name = o.value(QStringLiteral("name")).toString();
        e.model = o.value(QStringLiteral("model")).toString();
        if (e.model != QLatin1String("slim"))
            e.model = QStringLiteral("classic");
        if (!e.id.isEmpty())
            m_skins.append(e);
    }
}

void ChangeSkinDialog::ensureDefaultSkin()
{
    const QString id = QString::fromLatin1(PolytechSkins::DEFAULT_SKIN_ID);
    QDir().mkpath(libSkinsDir());
    if (!QFile::exists(libPngFor(id))) {
        QFile::copy(QString::fromLatin1(PolytechSkins::DEFAULT_SKIN_RESOURCE), libPngFor(id));
        // a file copied out of the resources is read-only
        QFile::setPermissions(libPngFor(id), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    }
    for (const SkinEntry& e : m_skins) {
        if (e.id == id)
            return;
    }
    m_skins.prepend({ id, tr("Стив"), QStringLiteral("classic") });
    saveLibrary();
}

void ChangeSkinDialog::saveLibrary()
{
    QDir().mkpath(libDir());

    QJsonArray arr;
    for (const SkinEntry& e : m_skins) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        o.insert(QStringLiteral("name"), e.name);
        o.insert(QStringLiteral("model"), e.model);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("skins"), arr);

    QFile f(QDir(libDir()).filePath(QStringLiteral("library.json")));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }
}

// ---------------- per-instance active pointer ----------------

void ChangeSkinDialog::loadActive()
{
    m_activeId.clear();
    if (m_skinDir.isEmpty())
        return;
    QFile f(QDir(instSkinDir()).filePath(QStringLiteral("active.txt")));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_activeId = QString::fromUtf8(f.readAll()).trimmed();
        f.close();
    }
    // nothing chosen (or a skin that was removed): the built-in one is what is used
    bool known = false;
    for (const SkinEntry& e : m_skins)
        known = known || e.id == m_activeId;
    if (!known)
        m_activeId = QString::fromLatin1(PolytechSkins::DEFAULT_SKIN_ID);
}

void ChangeSkinDialog::saveActive()
{
    if (m_skinDir.isEmpty())
        return;
    QDir().mkpath(instSkinDir());
    QFile f(QDir(instSkinDir()).filePath(QStringLiteral("active.txt")));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(m_activeId.toUtf8());
        f.close();
    }
}

void ChangeSkinDialog::materializeToInstance(const QString& id)
{
    if (m_skinDir.isEmpty())
        return;
    int idx = -1;
    for (int i = 0; i < m_skins.size(); ++i)
        if (m_skins[i].id == id)
            idx = i;
    if (idx < 0)
        return;

    QDir().mkpath(instSkinDir());
    const QString dst = QDir(instSkinDir()).filePath(QStringLiteral("skin.png"));
    QFile::remove(dst);
    QFile::copy(libPngFor(id), dst);

    QFile mf(QDir(instSkinDir()).filePath(QStringLiteral("model.txt")));
    if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        mf.write(m_skins[idx].model.toUtf8());
        mf.close();
    }
}

// ---------------- mod presence ----------------

void ChangeSkinDialog::checkMod()
{
    bool found = false;
    if (!m_gameRoot.isEmpty()) {
        QDir mods(QDir(m_gameRoot).filePath(QStringLiteral("mods")));
        if (mods.exists()) {
            const QStringList jars = mods.entryList(QStringList() << QStringLiteral("*.jar"), QDir::Files);
            for (const QString& j : jars) {
                if (j.contains(QStringLiteral("polytechskins"), Qt::CaseInsensitive)) {
                    found = true;
                    break;
                }
            }
        }
    }
    m_modWarning->setVisible(!found);
}

// ---------------- list / preview ----------------

void ChangeSkinDialog::refreshList()
{
    int keep = m_list->currentRow();
    m_list->clear();
    for (const SkinEntry& e : m_skins) {
        auto* item = new QListWidgetItem(faceIcon(libPngFor(e.id)), e.name);
        if (e.id == m_activeId) {
            item->setForeground(Obsidium::ACCENT_COLOR);
            item->setText(e.name + tr("  (активный)"));
        }
        m_list->addItem(item);
    }

    if (m_skins.isEmpty()) {
        m_preview->clearSkin();
    } else {
        if (keep < 0) {
            // On open (nothing selected yet), preselect the active skin.
            keep = 0;
            for (int i = 0; i < m_skins.size(); ++i) {
                if (m_skins[i].id == m_activeId) {
                    keep = i;
                    break;
                }
            }
        } else if (keep >= m_skins.size()) {
            keep = m_skins.size() - 1;
        }
        m_list->setCurrentRow(keep);  // triggers onSelectionChanged -> updatePreview
    }

    bool has = !m_skins.isEmpty();
    updateButtons();
    m_applyBtn->setEnabled(has);
    m_classic->setEnabled(has);
    m_slim->setEnabled(has);
}

void ChangeSkinDialog::updatePreview()
{
    int idx = currentIndex();
    if (idx < 0) {
        m_preview->clearSkin();
        return;
    }

    QImage img(libPngFor(m_skins[idx].id));
    m_preview->setSkin(img, m_skins[idx].model == QLatin1String("slim"));

    m_updating = true;
    if (m_skins[idx].model == QLatin1String("slim"))
        m_slim->setChecked(true);
    else
        m_classic->setChecked(true);
    m_updating = false;
}

void ChangeSkinDialog::onSelectionChanged()
{
    updatePreview();
    updateButtons();
}

void ChangeSkinDialog::updateButtons()
{
    // the built-in skin can be chosen but not renamed or removed
    const int idx = currentIndex();
    const bool own = idx >= 0 && m_skins[idx].id != QLatin1String(PolytechSkins::DEFAULT_SKIN_ID);
    m_renameBtn->setEnabled(own);
    m_removeBtn->setEnabled(own);
}

void ChangeSkinDialog::onModelToggled()
{
    if (m_updating)
        return;
    int idx = currentIndex();
    if (idx < 0)
        return;
    m_skins[idx].model = m_slim->isChecked() ? QStringLiteral("slim") : QStringLiteral("classic");
    saveLibrary();
    m_preview->setSkin(QImage(libPngFor(m_skins[idx].id)), m_slim->isChecked());
    // Keep this instance's model.txt in sync if the edited skin is the active one here.
    if (m_skins[idx].id == m_activeId)
        materializeToInstance(m_activeId);
}

// ---------------- library edits ----------------

void ChangeSkinDialog::addSkin()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Выберите файл скина"), QString(), tr("Изображения PNG (*.png)"));
    if (file.isEmpty())
        return;

    bool ok = false;
    QString suggested = QFileInfo(file).completeBaseName();
    QString name = QInputDialog::getText(this, tr("Имя скина"), tr("Название:"), QLineEdit::Normal, suggested, &ok);
    if (!ok)
        return;

    addSkinFromFile(file, name);
}

bool ChangeSkinDialog::addSkinFromFile(const QString& file, const QString& name)
{
    if (!QDir().mkpath(libSkinsDir())) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось создать папку библиотеки скинов."));
        return false;
    }

    // Unique id even when several files are added within the same millisecond.
    const qint64 base = QDateTime::currentMSecsSinceEpoch();
    QString id = QString::number(base);
    for (int n = 1; QFile::exists(libPngFor(id)); ++n)
        id = QString::number(base) + QStringLiteral("_") + QString::number(n);

    SkinEntry e;
    e.id = id;
    e.name = name.trimmed().isEmpty() ? QFileInfo(file).completeBaseName() : name.trimmed();
    e.model = QStringLiteral("classic");

    if (!QFile::copy(file, libPngFor(e.id))) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось скопировать файл скина."));
        return false;
    }

    m_skins.append(e);
    saveLibrary();
    refreshList();
    m_list->setCurrentRow(m_skins.size() - 1);
    return true;
}

void ChangeSkinDialog::renameSkin()
{
    int idx = currentIndex();
    if (idx < 0)
        return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Переименовать скин"), tr("Новое название:"), QLineEdit::Normal,
                                         m_skins[idx].name, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    m_skins[idx].name = name.trimmed();
    saveLibrary();
    refreshList();
}

void ChangeSkinDialog::removeSkin()
{
    int idx = currentIndex();
    if (idx < 0)
        return;
    if (QMessageBox::question(this, tr("Удалить скин"),
                              tr("Удалить «%1» из общей библиотеки?").arg(m_skins[idx].name)) != QMessageBox::Yes)
        return;

    QString id = m_skins[idx].id;
    QFile::remove(libPngFor(id));
    m_skins.removeAt(idx);
    if (m_activeId == id) {
        // back to the built-in skin
        m_activeId = QString::fromLatin1(PolytechSkins::DEFAULT_SKIN_ID);
        saveActive();
        materializeToInstance(m_activeId);
    }
    saveLibrary();
    refreshList();
}

void ChangeSkinDialog::applySkin()
{
    int idx = currentIndex();
    if (idx < 0)
        return;
    m_activeId = m_skins[idx].id;
    saveActive();
    materializeToInstance(m_activeId);
    refreshList();
}

// ---------------- drag & drop ----------------

void ChangeSkinDialog::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls())
        return;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && url.toLocalFile().endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
    }
}

void ChangeSkinDialog::dropEvent(QDropEvent* event)
{
    int added = 0;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
            continue;
        if (addSkinFromFile(path, QFileInfo(path).completeBaseName()))
            ++added;
    }
    if (added > 0)
        event->acceptProposedAction();
}

void ChangeSkinDialog::updateApplyText()
{
    if (m_nick.isEmpty()) {
        m_applyBtn->setText(tr("Применить к этой сборке"));
        return;
    }
    // only the nick is shortened, from the right: "Выбрать для ника VeryLongNi…"
    m_applyBtn->ensurePolished();
    const QFontMetrics metrics(m_applyBtn->font());
    const QString prefix = tr("Выбрать для ника %1").arg(QString());
    const int room = m_applyBtn->width() - 28 - metrics.horizontalAdvance(prefix);  // padding and frame of buttonCss()
    m_applyBtn->setText(prefix + metrics.elidedText(m_nick, Qt::ElideRight, qMax(room, metrics.horizontalAdvance(QStringLiteral("…")))));
    m_applyBtn->setToolTip(tr("Выбрать для ника %1").arg(m_nick));
}

void ChangeSkinDialog::showEvent(QShowEvent* event)
{
    ObsidiumDialog::showEvent(event);
    updateApplyText();
}

void ChangeSkinDialog::resizeEvent(QResizeEvent* event)
{
    ObsidiumDialog::resizeEvent(event);
    updateApplyText();
}
