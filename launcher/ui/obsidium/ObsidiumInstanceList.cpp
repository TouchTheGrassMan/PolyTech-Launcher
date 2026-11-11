// SPDX-License-Identifier: GPL-3.0-only
#include "ObsidiumInstanceList.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QIcon>
#include <QItemSelectionModel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QTimer>

#include <algorithm>

#include "Application.h"
#include "InstanceList.h"
#include "settings/SettingsObject.h"
#include "ui/obsidium/ObsidiumFrame.h"

namespace {

const QColor TEXT(233, 221, 201);
const QColor TEXT_DIM(190, 176, 200);
const QColor ACCENT(214, 120, 255);
const char* const ORDER_SETTING = "ObsidiumListOrder";
const char* const ROW_MIME = "application/x-obsidium-row";

/// The gear, 13x13 cells: X is the metal.
const int GEAR_CELLS = 13;
const char* const GEAR[GEAR_CELLS] = {
    ".....XXX.....",  //
    "..XX.XXX.XX..",  //
    "..XXXXXXXXX..",  //
    "...XXXXXXX...",  //
    ".XXXX...XXXX.",  //
    "XXXX.....XXXX",  //
    "XXXX.....XXXX",  //
    "XXXX.....XXXX",  //
    ".XXXX...XXXX.",  //
    "...XXXXXXX...",  //
    "..XXXXXXXXX..",  //
    "..XX.XXX.XX..",  //
    ".....XXX.....",  //
};

void paintGear(QPainter* p, const QRect& area, bool hot)
{
    const int cell = qMax(1, qRound(qMin(area.width(), area.height()) / qreal(GEAR_CELLS)));
    const int size = cell * GEAR_CELLS;
    const QPoint origin(area.x() + (area.width() - size) / 2, area.y() + (area.height() - size) / 2);
    const QColor shadow(0, 0, 0, 170);
    const QColor metal = hot ? QColor(242, 196, 255) : QColor(196, 176, 214);
    const QColor rim = hot ? ACCENT : QColor(120, 92, 140);
    auto metalAt = [](int x, int y) { return x >= 0 && y >= 0 && x < GEAR_CELLS && y < GEAR_CELLS && GEAR[y][x] == 'X'; };
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < GEAR_CELLS; y++) {
            for (int x = 0; x < GEAR_CELLS; x++) {
                if (!metalAt(x, y))
                    continue;
                const QRect r(origin.x() + x * cell, origin.y() + y * cell, cell, cell);
                if (pass == 0) {
                    p->fillRect(r.translated(cell, cell), shadow);  // a hard shadow, as on the picture's buttons
                } else {
                    // the cells next to the hole or the outside are the rim
                    const bool edge = !metalAt(x, y - 1) || !metalAt(x, y + 1) || !metalAt(x - 1, y) || !metalAt(x + 1, y);
                    p->fillRect(r, edge ? rim : metal);
                }
            }
        }
    }
}

/// The group's fold arrow, pixel style: pointing right (folded) or down (open).
void paintArrow(QPainter* p, const QRect& area, bool open, const QColor& color)
{
    const int cell = qMax(1, qRound(qMin(area.width(), area.height()) / 7.0));
    const QPoint o(area.x() + (area.width() - cell * 7) / 2, area.y() + (area.height() - cell * 7) / 2);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < 4; i++) {
            // a triangle of 7, 5, 3, 1 cells
            for (int j = i; j < 7 - i; j++) {
                const QPoint at = open ? QPoint(j, i + 2) : QPoint(i + 2, j);
                const QRect r(o.x() + at.x() * cell, o.y() + at.y() * cell, cell, cell);
                if (pass == 0)
                    p->fillRect(r.translated(cell, cell), QColor(0, 0, 0, 170));
                else
                    p->fillRect(r, color);
            }
        }
    }
}

void drawShadowedText(QPainter* p, const QRect& r, int flags, const QString& text, const QColor& color, int shadow)
{
    p->setPen(QColor(0, 0, 0, 200));
    p->drawText(r.translated(shadow, shadow), flags, text);
    p->setPen(color);
    p->drawText(r, flags, text);
}

bool isGroupKey(const QString& key)
{
    return key.startsWith(QStringLiteral("group/"));
}

}  // namespace

// ---- ObsidiumInstanceTree ------------------------------------------------------------------

ObsidiumInstanceTree::ObsidiumInstanceTree(QSortFilterProxyModel* source, QObject* parent) : QAbstractItemModel(parent), m_source(source)
{
    connect(source, &QAbstractItemModel::modelReset, this, &ObsidiumInstanceTree::scheduleRebuild);
    connect(source, &QAbstractItemModel::layoutChanged, this, &ObsidiumInstanceTree::scheduleRebuild);
    connect(source, &QAbstractItemModel::rowsInserted, this, &ObsidiumInstanceTree::scheduleRebuild);
    connect(source, &QAbstractItemModel::rowsRemoved, this, &ObsidiumInstanceTree::scheduleRebuild);
    connect(source, &QAbstractItemModel::rowsMoved, this, &ObsidiumInstanceTree::scheduleRebuild);
    connect(source, &QAbstractItemModel::dataChanged, this, &ObsidiumInstanceTree::scheduleRebuild);
    rebuild();
}

void ObsidiumInstanceTree::scheduleRebuild()
{
    if (m_rebuildQueued)
        return;
    m_rebuildQueued = true;
    QTimer::singleShot(0, this, &ObsidiumInstanceTree::rebuild);
}

QStringList ObsidiumInstanceTree::structureOf(const Node& root) const
{
    QStringList out;
    for (const auto& child : root.children) {
        out << (child->group ? groupKey(child->key) : child->key);
        for (const auto& inner : child->children)
            out << QStringLiteral(">") + inner->key;
    }
    return out;
}

void ObsidiumInstanceTree::rebuild()
{
    m_rebuildQueued = false;

    // the order the user dragged things into; what it doesn't know goes first (instances without a group in
    // the source's order, then the groups by name), so that a new instance shows at the top
    const QStringList order = APPLICATION->settings()->get(ORDER_SETTING).toStringList();
    QHash<QString, int> position;
    for (int i = 0; i < order.size(); i++)
        position.insert(order[i], i);

    Node root;
    QHash<QString, Node*> groups;
    QHash<QString, int> sourceRows;
    QHash<const Node*, qint64> rank;
    const qint64 KNOWN = 1'000'000'000;
    for (int row = 0; row < m_source->rowCount(); row++) {
        const QModelIndex src = m_source->index(row, 0);
        const QString id = src.data(InstanceList::InstanceIDRole).toString();
        const QString group = src.data(InstanceList::GroupRole).toString();
        sourceRows.insert(id, row);
        Node* parentNode = &root;
        if (!group.isEmpty()) {
            Node*& g = groups[group];
            if (!g) {
                auto made = std::make_unique<Node>();
                made->group = true;
                made->key = group;
                made->name = group;
                made->parent = &root;
                g = made.get();
                root.children.push_back(std::move(made));
            }
            parentNode = g;
        }
        auto instance = std::make_unique<Node>();
        instance->key = id;
        instance->parent = parentNode;
        rank.insert(instance.get(), position.contains(id) ? KNOWN + position.value(id) : row);
        parentNode->children.push_back(std::move(instance));
    }
    QStringList names = groups.keys();
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return a.localeAwareCompare(b) < 0; });
    for (int i = 0; i < names.size(); i++) {
        const QString key = groupKey(names[i]);
        rank.insert(groups.value(names[i]), position.contains(key) ? KNOWN + position.value(key) : KNOWN / 2 + i);
    }
    auto byRank = [&rank](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) { return rank.value(a.get()) < rank.value(b.get()); };
    std::stable_sort(root.children.begin(), root.children.end(), byRank);
    for (auto& child : root.children)
        std::stable_sort(child->children.begin(), child->children.end(), byRank);

    m_sourceRows = sourceRows;
    if (structureOf(root) == structureOf(m_root)) {
        // the same rows: only what they show may have changed (a name, an icon)
        for (const auto& child : m_root.children) {
            if (!child->children.empty()) {
                const QModelIndex parentIndex = indexOf(child.get());
                emit dataChanged(index(0, 0, parentIndex), index(int(child->children.size()) - 1, 0, parentIndex));
            }
        }
        if (!m_root.children.empty())
            emit dataChanged(index(0, 0), index(int(m_root.children.size()) - 1, 0));
        return;
    }
    beginResetModel();
    m_root.children.clear();
    for (auto& child : root.children) {
        child->parent = &m_root;
        m_root.children.push_back(std::move(child));
    }
    endResetModel();
    emit rebuilt();
}

void ObsidiumInstanceTree::saveOrder() const
{
    QStringList order;
    for (const auto& child : m_root.children) {
        order << (child->group ? groupKey(child->key) : child->key);
        for (const auto& inner : child->children)
            order << inner->key;
    }
    APPLICATION->settings()->set(ORDER_SETTING, order);
}

void ObsidiumInstanceTree::move(const QString& movedKey, const QString& group, const QString& before)
{
    // the arrangement as keys: the top level, and each group's instances
    QStringList top;
    QHash<QString, QStringList> members;
    for (const auto& child : m_root.children) {
        top << (child->group ? groupKey(child->key) : child->key);
        if (child->group) {
            for (const auto& inner : child->children)
                members[child->key] << inner->key;
        }
    }
    const bool movingGroup = isGroupKey(movedKey);
    if (movingGroup && !group.isEmpty())
        return;  // groups don't nest
    QStringList& target = group.isEmpty() ? top : members[group];
    int at = before.isEmpty() ? int(target.size()) : int(target.indexOf(before));
    if (at < 0)
        at = int(target.size());
    const int was = int(target.indexOf(movedKey));
    if (was >= 0 && was < at)
        at--;
    top.removeAll(movedKey);
    for (auto& list : members)
        list.removeAll(movedKey);
    target.insert(qBound(0, at, int(target.size())), movedKey);

    if (!movingGroup) {
        const QString current = APPLICATION->instances()->getInstanceGroup(movedKey);
        if (current != group) {
            if (!group.isEmpty() && !top.contains(groupKey(group)))
                top.append(groupKey(group));
            APPLICATION->instances()->setInstanceGroup(movedKey, group.isEmpty() ? QString() : group);
        }
    }
    QStringList order;
    for (const auto& key : top) {
        order << key;
        if (isGroupKey(key))
            order << members.value(key.mid(6));
    }
    APPLICATION->settings()->set(ORDER_SETTING, order);
    rebuild();
}

ObsidiumInstanceTree::Node* ObsidiumInstanceTree::node(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<Node*>(index.internalPointer()) : const_cast<Node*>(&m_root);
}

QModelIndex ObsidiumInstanceTree::indexOf(const Node* n) const
{
    if (!n || n == &m_root || !n->parent)
        return {};
    const auto& siblings = n->parent->children;
    for (size_t i = 0; i < siblings.size(); i++) {
        if (siblings[i].get() == n)
            return createIndex(int(i), 0, const_cast<Node*>(n));
    }
    return {};
}

QModelIndex ObsidiumInstanceTree::index(int row, int column, const QModelIndex& parentIndex) const
{
    const Node* p = node(parentIndex);
    if (column != 0 || row < 0 || row >= int(p->children.size()))
        return {};
    return createIndex(row, 0, p->children[size_t(row)].get());
}

QModelIndex ObsidiumInstanceTree::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};
    return indexOf(node(child)->parent);
}

int ObsidiumInstanceTree::rowCount(const QModelIndex& parentIndex) const
{
    if (parentIndex.column() > 0)
        return 0;
    return int(node(parentIndex)->children.size());
}

int ObsidiumInstanceTree::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant ObsidiumInstanceTree::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    const Node* n = node(index);
    if (role == IsGroupRole)
        return n->group;
    if (role == KeyRole)
        return n->group ? groupKey(n->key) : n->key;
    if (n->group) {
        switch (role) {
            case Qt::DisplayRole:
            case Qt::EditRole:
            case GroupNameRole:
                return n->name;
            case ChildCountRole:
                return int(n->children.size());
            default:
                return {};
        }
    }
    return sourceIndex(index).data(role);
}

bool ObsidiumInstanceTree::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || isGroup(index))
        return false;
    return m_source->setData(sourceIndex(index), value, role);
}

Qt::ItemFlags ObsidiumInstanceTree::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    if (isGroup(index))
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

bool ObsidiumInstanceTree::isGroup(const QModelIndex& index) const
{
    return index.isValid() && node(index)->group;
}

QString ObsidiumInstanceTree::key(const QModelIndex& index) const
{
    return index.data(KeyRole).toString();
}

QModelIndex ObsidiumInstanceTree::indexOfKey(const QString& wanted) const
{
    for (const auto& child : m_root.children) {
        if ((child->group ? groupKey(child->key) : child->key) == wanted)
            return indexOf(child.get());
        for (const auto& inner : child->children) {
            if (inner->key == wanted)
                return indexOf(inner.get());
        }
    }
    return {};
}

QModelIndex ObsidiumInstanceTree::sourceIndex(const QModelIndex& index) const
{
    if (!index.isValid() || isGroup(index))
        return {};
    const QString id = node(index)->key;
    // the source's row of the instance, checked: the source may have moved since the last rebuild
    const int row = m_sourceRows.value(id, -1);
    if (row >= 0 && row < m_source->rowCount()) {
        const QModelIndex at = m_source->index(row, 0);
        if (at.data(InstanceList::InstanceIDRole).toString() == id)
            return at;
    }
    for (int r = 0; r < m_source->rowCount(); r++) {
        const QModelIndex at = m_source->index(r, 0);
        if (at.data(InstanceList::InstanceIDRole).toString() == id)
            return at;
    }
    return {};
}

QModelIndex ObsidiumInstanceTree::fromSource(const QModelIndex& source) const
{
    if (!source.isValid())
        return {};
    return indexOfKey(source.data(InstanceList::InstanceIDRole).toString());
}

// ---- ObsidiumInstanceDelegate --------------------------------------------------------------

ObsidiumInstanceDelegate::ObsidiumInstanceDelegate(ObsidiumInstanceList* list) : QStyledItemDelegate(list), m_list(list) {}

QSize ObsidiumInstanceDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const bool group = index.data(ObsidiumInstanceTree::IsGroupRole).toBool();
    return { qMax(1, option.rect.width()), qRound((group ? 38 : 48) * m_list->scale()) };
}

QRect ObsidiumInstanceDelegate::body(const QRect& rect, const QModelIndex& index) const
{
    const qreal s = m_list->scale();
    const int gap = qMax(1, qRound(2 * s));
    // a group's instances stand a little to the right of its header
    const int indent = index.parent().isValid() ? qRound(22 * s) : 1;
    return rect.adjusted(indent, gap, -2, -gap);
}

QRect ObsidiumInstanceDelegate::gearRect(const QRect& rect, const QModelIndex& index) const
{
    const qreal s = m_list->scale();
    const QRect b = body(rect, index);
    const bool group = index.data(ObsidiumInstanceTree::IsGroupRole).toBool();
    const int side = qRound((group ? 28 : 34) * s);
    return { b.right() - side - qRound(11 * s), b.y() + (b.height() - side) / 2, side, side };
}

QRect ObsidiumInstanceDelegate::nameRect(const QRect& rect, const QModelIndex& index) const
{
    const qreal s = m_list->scale();
    const QRect b = body(rect, index);
    const int pad = qRound(8 * s);
    const int lead = qRound((index.data(ObsidiumInstanceTree::IsGroupRole).toBool() ? 18 : 32) * s);  // the arrow, or the icon
    const int left = b.x() + pad + lead + pad;
    return { left, b.y(), gearRect(rect, index).left() - pad - left, b.height() };
}

void ObsidiumInstanceDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QRect name = nameRect(option.rect, index);
    const int height = qMin(name.height(), qRound(30 * m_list->scale()));
    editor->setFont(Obsidium::pixelFont(17 * m_list->scale()));  // the name is edited in the font it is shown in
    editor->setGeometry(name.x() - 4, name.y() + (name.height() - height) / 2, name.width() + 4, height);
}

void ObsidiumInstanceDelegate::paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const qreal s = m_list->scale();
    const QRect row = body(option.rect, index);
    const bool group = index.data(ObsidiumInstanceTree::IsGroupRole).toBool();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover = option.state & QStyle::State_MouseOver;
    const int pad = qRound(8 * s);
    const int shadow = qMax(1, qRound(2 * s));

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(row).adjusted(0.5, 0.5, -0.5, -0.5), 4 * s, 4 * s);
    if (group) {
        p->fillPath(shape, hover ? QColor(48, 24, 64, 200) : QColor(20, 10, 28, 150));
        p->setPen(QPen(hover ? QColor(150, 84, 196) : QColor(66, 44, 84), qMax<qreal>(1.0, 1.5 * s), Qt::DashLine));
    } else {
        p->fillPath(shape, selected ? QColor(96, 40, 132, 225) : hover ? QColor(58, 28, 78, 215) : QColor(26, 14, 34, 200));
        p->setPen(QPen(selected ? ACCENT : hover ? QColor(150, 84, 196) : QColor(84, 56, 104), qMax<qreal>(1.0, 2 * s)));
    }
    p->drawPath(shape);
    p->setRenderHint(QPainter::Antialiasing, false);

    const QRect name = nameRect(option.rect, index);
    if (group) {
        const int side = qRound(18 * s);
        paintArrow(p, QRect(row.x() + pad, row.y() + (row.height() - side) / 2, side, side), option.state & QStyle::State_Open,
                   hover ? ACCENT : QColor(196, 150, 228));
        const QFont font = Obsidium::pixelFont(16 * s);
        p->setFont(font);
        const QFontMetrics metrics(font);
        const QString count = QStringLiteral("  (%1)").arg(index.data(ObsidiumInstanceTree::ChildCountRole).toInt());
        const int countWidth = metrics.horizontalAdvance(count);
        const QString title = metrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, qMax(0, name.width() - countWidth));
        drawShadowedText(p, name, Qt::AlignLeft | Qt::AlignVCenter, title, QColor(222, 190, 255), shadow);
        const QRect countRect(name.x() + metrics.horizontalAdvance(title), name.y(), countWidth, name.height());
        drawShadowedText(p, countRect, Qt::AlignLeft | Qt::AlignVCenter, count, TEXT_DIM, shadow);
    } else {
        const int iconSide = qRound(32 * s);
        const QRect iconRect(row.x() + pad, row.y() + (row.height() - iconSide) / 2, iconSide, iconSide);
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull())
            icon.paint(p, iconRect, Qt::AlignCenter);
        const QFont font = Obsidium::pixelFont(17 * s);
        p->setFont(font);
        const QString text = QFontMetrics(font).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, name.width());
        drawShadowedText(p, name, Qt::AlignLeft | Qt::AlignVCenter, text, selected ? QColor(Qt::white) : TEXT, shadow);
    }
    paintGear(p, gearRect(option.rect, index), m_list->hoveredGear() == index);
    p->restore();
}

// ---- ObsidiumInstanceList ------------------------------------------------------------------

ObsidiumInstanceList::ObsidiumInstanceList(QWidget* parent) : QTreeView(parent), m_delegate(new ObsidiumInstanceDelegate(this))
{
    setItemDelegate(m_delegate);
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setIndentation(0);
    setItemsExpandable(true);
    setExpandsOnDoubleClick(false);
    setAnimated(false);
    setUniformRowHeights(false);
    setAllColumnsShowFocus(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoScroll(false);  // (it would open a folded group to show its chosen instance)
    viewport()->setAttribute(Qt::WA_Hover, true);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
    setDragEnabled(false);  // rows are dragged by startRowDrag()
    setDropIndicatorShown(false);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);

    // Enter on a row: an instance starts, a group folds or opens
    connect(this, &QAbstractItemView::activated, this, [this](const QModelIndex& index) {
        if (!m_tree)
            return;
        if (m_tree->isGroup(index))
            setExpanded(index, !isExpanded(index));
        else
            emit instanceActivated(m_tree->sourceIndex(index));
    });
    // a group folded or opened (a click, the keyboard) is remembered as the classic view remembered it
    auto remember = [this](const QModelIndex& index, bool collapsed) {
        if (m_restoring || !m_tree || !m_tree->isGroup(index))
            return;
        APPLICATION->instances()->on_GroupStateChanged(index.data(ObsidiumInstanceTree::GroupNameRole).toString(), collapsed);
    };
    connect(this, &QTreeView::expanded, this, [remember](const QModelIndex& index) { remember(index, false); });
    connect(this, &QTreeView::collapsed, this, [remember](const QModelIndex& index) { remember(index, true); });
}

void ObsidiumInstanceList::setSource(QSortFilterProxyModel* source, QItemSelectionModel* sourceSelection)
{
    m_source = source;
    m_sourceSelection = sourceSelection;
    m_tree = new ObsidiumInstanceTree(source, this);
    setModel(m_tree);
    connect(m_tree, &ObsidiumInstanceTree::rebuilt, this, &ObsidiumInstanceList::restoreState);
    connect(sourceSelection, &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current) { syncFromSource(current); });
    restoreState();
}

void ObsidiumInstanceList::setScale(qreal factor)
{
    if (factor <= 0)
        return;
    m_scale = factor;
    doItemsLayout();
    viewport()->update();
}

void ObsidiumInstanceList::restoreState()
{
    if (!m_tree)
        return;
    m_restoring = true;
    for (int row = 0; row < m_tree->rowCount(); row++) {
        const QModelIndex index = m_tree->index(row, 0);
        if (m_tree->isGroup(index))
            setExpanded(index, !APPLICATION->instances()->isGroupCollapsed(index.data(ObsidiumInstanceTree::GroupNameRole).toString()));
    }
    m_restoring = false;
    syncFromSource(m_sourceSelection ? m_sourceSelection->currentIndex() : QModelIndex());
}

void ObsidiumInstanceList::syncFromSource(const QModelIndex& sourceCurrent)
{
    if (m_syncing || !m_tree)
        return;
    m_syncing = true;
    const QModelIndex index = m_tree->fromSource(sourceCurrent);
    if (index.isValid()) {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        // shown when it is not inside a folded group (scrolling there would open it)
        if (!index.parent().isValid() || isExpanded(index.parent()))
            scrollTo(index);
    } else {
        selectionModel()->clearSelection();
    }
    m_syncing = false;
}

void ObsidiumInstanceList::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QTreeView::currentChanged(current, previous);
    if (m_syncing || !m_tree || !m_sourceSelection || !current.isValid() || m_tree->isGroup(current))
        return;
    const QModelIndex source = m_tree->sourceIndex(current);
    if (source.isValid() && m_sourceSelection->currentIndex() != source) {
        m_syncing = true;
        m_sourceSelection->setCurrentIndex(source, QItemSelectionModel::ClearAndSelect);
        m_syncing = false;
    }
}

void ObsidiumInstanceList::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_Open | QStyle::State_HasFocus);
    if (selectionModel() && selectionModel()->isSelected(index))
        opt.state |= QStyle::State_Selected;
    if (index == m_hovered)
        opt.state |= QStyle::State_MouseOver;
    if (isExpanded(index))
        opt.state |= QStyle::State_Open;
    m_delegate->paint(painter, opt, index);
}

QModelIndex ObsidiumInstanceList::firstInstance() const
{
    if (!m_tree)
        return {};
    m_tree->flush();  // (the source has just been filtered differently, as a tab was chosen)
    for (int row = 0; row < m_tree->rowCount(); row++) {
        const QModelIndex top = m_tree->index(row, 0);
        if (!m_tree->isGroup(top))
            return m_tree->sourceIndex(top);
        // a group's instances count when it is open
        if (isExpanded(top) && m_tree->rowCount(top) > 0)
            return m_tree->sourceIndex(m_tree->index(0, 0, top));
    }
    return {};
}

bool ObsidiumInstanceList::onGear(const QPoint& pos, QModelIndex* index) const
{
    const QModelIndex at = indexAt(pos);
    if (!at.isValid())
        return false;
    if (index)
        *index = at;
    return m_delegate->gearRect(visualRect(at), at).contains(pos);
}

QString ObsidiumInstanceList::nextSibling(const QModelIndex& index) const
{
    const QModelIndex next = index.sibling(index.row() + 1, 0);
    return next.isValid() ? m_tree->key(next) : QString();
}

ObsidiumInstanceList::DropTarget ObsidiumInstanceList::dropTargetAt(const QPoint& pos, const QString& dragged) const
{
    DropTarget t;
    if (!m_tree)
        return t;
    const int thick = qMax(2, qRound(3 * m_scale));
    const bool draggingGroup = isGroupKey(dragged);
    auto lineUnder = [&](const QModelIndex& index, bool bottom) {
        const QRect b = m_delegate->body(visualRect(index), index);
        const int y = bottom ? visualRect(index).bottom() : visualRect(index).top();
        return QRect(b.left(), y - thick / 2, b.width(), thick);
    };
    // the last row of a group's block: the group itself when it is folded or empty
    auto blockEnd = [&](const QModelIndex& groupIndex) {
        const int children = m_tree->rowCount(groupIndex);
        return isExpanded(groupIndex) && children > 0 ? m_tree->index(children - 1, 0, groupIndex) : groupIndex;
    };
    // after a top-level row (a group: after its block)
    auto afterTop = [&](const QModelIndex& index) {
        t.valid = true;
        t.group.clear();
        t.before = nextSibling(index);
        const QModelIndex last = m_tree->isGroup(index) ? blockEnd(index) : index;
        t.line = lineUnder(last, true);
        t.line.setLeft(m_delegate->body(visualRect(index), index).left());
    };
    auto beforeRow = [&](const QModelIndex& index, const QString& group) {
        t.valid = true;
        t.group = group;
        t.before = m_tree->key(index);
        t.line = lineUnder(index, false);
    };

    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) {
        // below everything: the end of the list
        const int rows = m_tree->rowCount();
        if (rows == 0) {
            t.valid = true;
            return t;
        }
        afterTop(m_tree->index(rows - 1, 0));
        return t;
    }
    if (m_tree->key(index) == dragged)
        return t;  // onto itself: nothing to do
    const QRect r = visualRect(index);
    const int y = pos.y() - r.top();
    const int h = qMax(1, r.height());

    if (m_tree->isGroup(index)) {
        const QString name = index.data(ObsidiumInstanceTree::GroupNameRole).toString();
        if (draggingGroup) {
            if (y < h / 2)
                beforeRow(index, QString());
            else
                afterTop(index);
            return t;
        }
        if (y < h / 4) {
            beforeRow(index, QString());
        } else if (y > h * 3 / 4 && !isExpanded(index)) {
            afterTop(index);
        } else {
            // into the group: at its start when the mouse is just above its first instance, else at its end
            t.valid = true;
            t.group = name;
            t.before = (y > h * 3 / 4 && m_tree->rowCount(index) > 0) ? m_tree->key(m_tree->index(0, 0, index)) : QString();
            t.box = m_delegate->body(r, index);
        }
        return t;
    }

    const QModelIndex parentIndex = index.parent();
    if (draggingGroup) {
        // a group goes among the top-level rows: before or after the one the mouse is over
        const QModelIndex topIndex = parentIndex.isValid() ? parentIndex : index;
        if (!parentIndex.isValid() && y < h / 2)
            beforeRow(topIndex, QString());
        else
            afterTop(topIndex);
        return t;
    }
    const QString group = parentIndex.isValid() ? parentIndex.data(ObsidiumInstanceTree::GroupNameRole).toString() : QString();
    if (y < h / 2) {
        beforeRow(index, group);
    } else {
        t.valid = true;
        t.group = group;
        t.before = nextSibling(index);
        t.line = lineUnder(index, true);
    }
    if (t.before == dragged)
        t.valid = false;  // just after itself: where it already is
    return t;
}

void ObsidiumInstanceList::paintEvent(QPaintEvent* event)
{
    QTreeView::paintEvent(event);
    QPainter p(viewport());
    if (m_drop.valid) {
        // where the dragged row goes: a line between rows, or a frame around the group it goes into
        if (!m_drop.box.isNull()) {
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(ACCENT, qMax<qreal>(2.0, 2.5 * m_scale)));
            p.setBrush(QColor(214, 120, 255, 40));
            p.drawRoundedRect(QRectF(m_drop.box).adjusted(1, 1, -1, -1), 4 * m_scale, 4 * m_scale);
        } else if (!m_drop.line.isNull()) {
            p.fillRect(m_drop.line, ACCENT);
            const int knob = m_drop.line.height() * 2;
            p.fillRect(QRect(m_drop.line.left(), m_drop.line.center().y() - knob / 2, knob, knob), ACCENT);
        }
    }
    if (!model() || model()->rowCount() > 0)
        return;
    // nothing yet: what to do about it, in the launcher's font
    const QRect area = viewport()->rect().adjusted(qRound(24 * m_scale), 0, -qRound(24 * m_scale), 0);
    const QString title = tr("Здесь пока пусто");
    const QString hint = tr("Нажмите «Добавить сборку», чтобы установить сборку или создать свою.");
    const QFont titleFont = Obsidium::pixelFont(24 * m_scale);
    const QFont hintFont = Obsidium::pixelFont(15 * m_scale);
    const int gap = qRound(12 * m_scale);
    const QRect titleBox = QFontMetrics(titleFont).boundingRect(area, Qt::AlignHCenter | Qt::TextWordWrap, title);
    const QRect hintBox = QFontMetrics(hintFont).boundingRect(area, Qt::AlignHCenter | Qt::TextWordWrap, hint);
    int top = area.center().y() - (titleBox.height() + gap + hintBox.height()) / 2;
    const int shadow = qMax(1, qRound(2 * m_scale));
    p.setFont(titleFont);
    drawShadowedText(&p, QRect(area.x(), top, area.width(), titleBox.height()), Qt::AlignHCenter | Qt::TextWordWrap, title, TEXT, shadow);
    top += titleBox.height() + gap;
    p.setFont(hintFont);
    drawShadowedText(&p, QRect(area.x(), top, area.width(), hintBox.height()), Qt::AlignHCenter | Qt::TextWordWrap, hint, TEXT_DIM, shadow);
}

void ObsidiumInstanceList::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    QModelIndex index;
    m_pressed = QModelIndex();
    if (event->button() == Qt::LeftButton && onGear(pos, &index)) {
        const QPoint below = viewport()->mapToGlobal(m_delegate->gearRect(visualRect(index), index).bottomLeft());
        if (m_tree->isGroup(index)) {
            emit groupActionsRequested(index.data(ObsidiumInstanceTree::GroupNameRole).toString(), below);
        } else {
            setCurrentIndex(index);
            emit actionsRequested(below);
        }
        event->accept();
        return;
    }
    index = indexAt(pos);
    if (event->button() == Qt::LeftButton) {
        m_pressed = index;
        m_pressPos = pos;
    }
    // a group's header isn't chosen like an instance: it folds on release (or is dragged)
    if (index.isValid() && m_tree && m_tree->isGroup(index)) {
        event->accept();
        return;
    }
    QTreeView::mousePressEvent(event);
}

void ObsidiumInstanceList::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if ((event->buttons() & Qt::LeftButton) && m_pressed.isValid()) {
        if ((pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            const QModelIndex index = m_pressed;
            m_pressed = QModelIndex();
            startRowDrag(index);
        }
        return;  // (no selecting by dragging over the rows)
    }
    QModelIndex index;
    const QModelIndex gear = onGear(pos, &index) ? index : QModelIndex();
    const QModelIndex hovered = indexAt(pos);
    if (gear != QModelIndex(m_hoveredGear) || hovered != QModelIndex(m_hovered)) {
        m_hoveredGear = gear;
        m_hovered = hovered;
        viewport()->update();
    }
    viewport()->setCursor(gear.isValid() || (hovered.isValid() && m_tree && m_tree->isGroup(hovered)) ? Qt::PointingHandCursor
                                                                                                        : Qt::ArrowCursor);
    QTreeView::mouseMoveEvent(event);
}

void ObsidiumInstanceList::mouseReleaseEvent(QMouseEvent* event)
{
    const QModelIndex pressed = m_pressed;
    m_pressed = QModelIndex();
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid() && m_tree && m_tree->isGroup(index)) {
        if (event->button() == Qt::LeftButton && pressed == index)
            setExpanded(index, !isExpanded(index));
        event->accept();
        return;
    }
    QTreeView::mouseReleaseEvent(event);
}

void ObsidiumInstanceList::mouseDoubleClickEvent(QMouseEvent* event)
{
    // two quick clicks on the gear open its menu, on a group they fold it (on release): neither starts the game
    const QPoint pos = event->position().toPoint();
    const QModelIndex index = indexAt(pos);
    event->accept();
    if (event->button() != Qt::LeftButton || !index.isValid() || onGear(pos) || !m_tree || m_tree->isGroup(index)) {
        if (index.isValid() && m_tree && m_tree->isGroup(index) && !onGear(pos))
            m_pressed = index;  // the release folds it again, as a second click
        return;
    }
    emit instanceActivated(m_tree->sourceIndex(index));
}

void ObsidiumInstanceList::leaveEvent(QEvent* event)
{
    if (m_hoveredGear.isValid() || m_hovered.isValid()) {
        m_hoveredGear = QModelIndex();
        m_hovered = QModelIndex();
        viewport()->update();
    }
    QTreeView::leaveEvent(event);
}

void ObsidiumInstanceList::contextMenuEvent(QContextMenuEvent* event)
{
    const QModelIndex index = indexAt(viewport()->mapFromGlobal(event->globalPos()));
    if (!index.isValid() || !m_tree)
        return;
    if (m_tree->isGroup(index)) {
        emit groupActionsRequested(index.data(ObsidiumInstanceTree::GroupNameRole).toString(), event->globalPos());
        return;
    }
    setCurrentIndex(index);
    emit actionsRequested(event->globalPos());
}

void ObsidiumInstanceList::startRowDrag(const QModelIndex& index)
{
    if (!m_tree || !index.isValid())
        return;
    const QString dragged = m_tree->key(index);
    // the row itself goes with the mouse
    const QRect rect = visualRect(index);
    const qreal dpr = devicePixelRatioF();
    QPixmap picture(rect.size() * dpr);
    picture.setDevicePixelRatio(dpr);
    picture.fill(Qt::transparent);
    {
        QPainter p(&picture);
        p.setOpacity(0.85);
        QStyleOptionViewItem opt;
        initViewItemOption(&opt);
        opt.rect = QRect(QPoint(0, 0), rect.size());
        opt.state |= QStyle::State_Selected;
        if (isExpanded(index))
            opt.state |= QStyle::State_Open;
        m_delegate->paint(&p, opt, index);
    }
    auto* mime = new QMimeData();
    mime->setData(ROW_MIME, dragged.toUtf8());
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(picture);
    drag->setHotSpot(m_pressPos - rect.topLeft());
    drag->exec(Qt::MoveAction, Qt::MoveAction);
    m_drop = DropTarget();
    viewport()->update();
}

void ObsidiumInstanceList::autoScroll(const QPoint& pos)
{
    const int margin = qRound(28 * m_scale);
    const int step = qMax(4, qRound(14 * m_scale));
    QScrollBar* bar = verticalScrollBar();
    if (pos.y() < margin)
        bar->setValue(bar->value() - step);
    else if (pos.y() > viewport()->height() - margin)
        bar->setValue(bar->value() + step);
}

void ObsidiumInstanceList::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(ROW_MIME) && event->source() == this) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ObsidiumInstanceList::dragMoveEvent(QDragMoveEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if (event->mimeData()->hasFormat(ROW_MIME) && event->source() == this) {
        autoScroll(pos);
        m_drop = dropTargetAt(pos, QString::fromUtf8(event->mimeData()->data(ROW_MIME)));
        viewport()->update();
        event->setDropAction(Qt::MoveAction);
        if (m_drop.valid)
            event->accept();
        else
            event->ignore();
        return;
    }
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void ObsidiumInstanceList::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_drop = DropTarget();
    viewport()->update();
    QTreeView::dragLeaveEvent(event);
}

void ObsidiumInstanceList::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasFormat(ROW_MIME) && event->source() == this) {
        const QString dragged = QString::fromUtf8(event->mimeData()->data(ROW_MIME));
        const DropTarget target = dropTargetAt(event->position().toPoint(), dragged);
        m_drop = DropTarget();
        viewport()->update();
        if (target.valid && m_tree) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
            // (after the drag is over: the model is laid out again)
            const QString group = target.group;
            const QString before = target.before;
            QTimer::singleShot(0, this, [this, dragged, group, before] {
                m_tree->move(dragged, group, before);
                // a group an instance was dropped into opens to show it
                if (!group.isEmpty()) {
                    const QModelIndex groupIndex = m_tree->indexOfKey(ObsidiumInstanceTree::groupKey(group));
                    if (groupIndex.isValid())
                        setExpanded(groupIndex, true);
                }
            });
        } else {
            event->ignore();
        }
        return;
    }
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        emit droppedURLs(event->mimeData()->urls());
        return;
    }
    event->ignore();
}
