#include "ChangeSkinDialog.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
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

#include "BaseInstance.h"
#include "SkinPreview3D.h"

// Front-of-head region in a 64x64 (or 64x32) skin, used for list thumbnails.
static QIcon faceIcon(const QString& pngPath)
{
    QImage img(pngPath);
    if (img.isNull())
        return QIcon();
    if (img.width() >= 16 && img.height() >= 16) {
        QImage face = img.copy(8, 8, 8, 8).scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation);
        return QIcon(QPixmap::fromImage(face));
    }
    return QIcon(QPixmap::fromImage(img.scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation)));
}

ChangeSkinDialog::ChangeSkinDialog(BaseInstance* instance, QWidget* parent) : QDialog(parent), m_instance(instance)
{
    setWindowTitle(tr("Скины"));
    setModal(true);
    setAcceptDrops(true);
    resize(560, 400);

    if (m_instance)
        m_gameRoot = m_instance->gameRoot();

    auto* main = new QHBoxLayout(this);

    // ---- Left: the library list + its buttons ----
    auto* leftBox = new QVBoxLayout();
    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(32, 32));
    leftBox->addWidget(m_list, 1);

    auto* listButtons = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("Добавить…"), this);
    m_renameBtn = new QPushButton(tr("Переименовать"), this);
    m_removeBtn = new QPushButton(tr("Удалить"), this);
    listButtons->addWidget(addBtn);
    listButtons->addWidget(m_renameBtn);
    listButtons->addWidget(m_removeBtn);
    leftBox->addLayout(listButtons);
    main->addLayout(leftBox, 1);

    // ---- Right: preview + model + apply ----
    auto* rightBox = new QVBoxLayout();

    m_preview = new SkinPreview3D(this);
    rightBox->addWidget(m_preview, 1);

    auto* modelGroup = new QGroupBox(tr("Модель"), this);
    auto* modelRow = new QHBoxLayout(modelGroup);
    m_classic = new QRadioButton(tr("Classic (Steve)"), modelGroup);
    m_slim = new QRadioButton(tr("Slim (Alex)"), modelGroup);
    m_classic->setChecked(true);
    modelRow->addWidget(m_classic);
    modelRow->addWidget(m_slim);
    rightBox->addWidget(modelGroup);

    m_applyBtn = new QPushButton(tr("Применить как активный"), this);
    rightBox->addWidget(m_applyBtn);

    m_hint = new QLabel(tr("Активный скин отмечен жирным — именно он отправляется на сервер."), this);
    m_hint->setWordWrap(true);
    rightBox->addWidget(m_hint);

    main->addLayout(rightBox, 1);

    // Close button under the right column.
    auto* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (auto* b = closeBox->button(QDialogButtonBox::Close))
        b->setText(tr("Закрыть"));
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rightBox->addWidget(closeBox);

    connect(addBtn, &QPushButton::clicked, this, &ChangeSkinDialog::addSkin);
    connect(m_renameBtn, &QPushButton::clicked, this, &ChangeSkinDialog::renameSkin);
    connect(m_removeBtn, &QPushButton::clicked, this, &ChangeSkinDialog::removeSkin);
    connect(m_applyBtn, &QPushButton::clicked, this, &ChangeSkinDialog::applySkin);
    connect(m_list, &QListWidget::currentRowChanged, this, &ChangeSkinDialog::onSelectionChanged);
    connect(m_classic, &QRadioButton::toggled, this, &ChangeSkinDialog::onModelToggled);
    connect(m_slim, &QRadioButton::toggled, this, &ChangeSkinDialog::onModelToggled);

    loadLibrary();
    refreshList();
}

QString ChangeSkinDialog::baseDir() const
{
    return QDir(m_gameRoot).filePath(QStringLiteral("polytechskins"));
}

QString ChangeSkinDialog::skinsDir() const
{
    return QDir(baseDir()).filePath(QStringLiteral("skins"));
}

QString ChangeSkinDialog::pngPathFor(const QString& id) const
{
    return QDir(skinsDir()).filePath(id + QStringLiteral(".png"));
}

int ChangeSkinDialog::currentIndex() const
{
    int row = m_list->currentRow();
    if (row < 0 || row >= m_skins.size())
        return -1;
    return row;
}

void ChangeSkinDialog::loadLibrary()
{
    m_skins.clear();
    m_activeId.clear();
    if (m_gameRoot.isEmpty())
        return;

    QFile f(QDir(baseDir()).filePath(QStringLiteral("skins.json")));
    if (!f.open(QIODevice::ReadOnly))
        return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();
    m_activeId = root.value(QStringLiteral("active")).toString();
    for (const QJsonValue& v : root.value(QStringLiteral("skins")).toArray()) {
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

void ChangeSkinDialog::saveLibrary()
{
    if (m_gameRoot.isEmpty())
        return;
    QDir().mkpath(baseDir());

    QJsonArray arr;
    for (const SkinEntry& e : m_skins) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        o.insert(QStringLiteral("name"), e.name);
        o.insert(QStringLiteral("model"), e.model);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("active"), m_activeId);
    root.insert(QStringLiteral("skins"), arr);

    QFile f(QDir(baseDir()).filePath(QStringLiteral("skins.json")));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }
}

void ChangeSkinDialog::materializeActive()
{
    // Copy the active skin into skin.png + model.txt (what the mod reads).
    if (m_activeId.isEmpty() || m_gameRoot.isEmpty())
        return;
    int idx = -1;
    for (int i = 0; i < m_skins.size(); ++i)
        if (m_skins[i].id == m_activeId)
            idx = i;
    if (idx < 0)
        return;

    QDir().mkpath(baseDir());
    QString dst = QDir(baseDir()).filePath(QStringLiteral("skin.png"));
    QFile::remove(dst);
    QFile::copy(pngPathFor(m_activeId), dst);

    QFile mf(QDir(baseDir()).filePath(QStringLiteral("model.txt")));
    if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        mf.write(m_skins[idx].model.toUtf8());
        mf.close();
    }
}

void ChangeSkinDialog::refreshList()
{
    int keep = m_list->currentRow();
    m_list->clear();
    for (const SkinEntry& e : m_skins) {
        auto* item = new QListWidgetItem(faceIcon(pngPathFor(e.id)), e.name);
        if (e.id == m_activeId) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
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
    m_renameBtn->setEnabled(has);
    m_removeBtn->setEnabled(has);
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

    QImage img(pngPathFor(m_skins[idx].id));
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
    // Rebuild the 3D preview so the arm width follows the new model.
    m_preview->setSkin(QImage(pngPathFor(m_skins[idx].id)), m_slim->isChecked());
    // If the edited skin is the active one, keep model.txt in sync.
    if (m_skins[idx].id == m_activeId)
        materializeActive();
}

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
    if (m_gameRoot.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось определить папку игры для этого экземпляра."));
        return false;
    }
    if (!QDir().mkpath(skinsDir())) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось создать папку для скинов."));
        return false;
    }

    // Unique id even when several files are added within the same millisecond.
    const qint64 base = QDateTime::currentMSecsSinceEpoch();
    QString id = QString::number(base);
    for (int n = 1; QFile::exists(pngPathFor(id)); ++n)
        id = QString::number(base) + QStringLiteral("_") + QString::number(n);

    SkinEntry e;
    e.id = id;
    e.name = name.trimmed().isEmpty() ? QFileInfo(file).completeBaseName() : name.trimmed();
    e.model = QStringLiteral("classic");

    if (!QFile::copy(file, pngPathFor(e.id))) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось скопировать файл скина."));
        return false;
    }

    m_skins.append(e);
    saveLibrary();
    refreshList();
    m_list->setCurrentRow(m_skins.size() - 1);
    return true;
}

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
                              tr("Удалить «%1» из библиотеки?").arg(m_skins[idx].name)) != QMessageBox::Yes)
        return;

    QString id = m_skins[idx].id;
    QFile::remove(pngPathFor(id));
    m_skins.removeAt(idx);
    if (m_activeId == id)
        m_activeId.clear();  // skin.png is left as-is until another is applied
    saveLibrary();
    refreshList();
}

void ChangeSkinDialog::applySkin()
{
    int idx = currentIndex();
    if (idx < 0)
        return;
    m_activeId = m_skins[idx].id;
    saveLibrary();
    materializeActive();
    refreshList();
}
