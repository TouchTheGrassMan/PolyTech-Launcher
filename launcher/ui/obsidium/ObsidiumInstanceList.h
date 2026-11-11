// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QUrl>

#include <memory>
#include <vector>

class QItemSelectionModel;
class QSortFilterProxyModel;
class ObsidiumInstanceList;

/// The instances of the classic view's model (InstanceProxyModel), arranged for the Obsidium list:
/// instances without a group and the groups at the top level, each group's instances inside it,
/// in the order the user dragged them into (the "ObsidiumListOrder" setting). Everything about
/// an instance (its name, icon, id) is the source model's.
class ObsidiumInstanceTree : public QAbstractItemModel {
    Q_OBJECT
   public:
    enum Roles { IsGroupRole = Qt::UserRole + 100, KeyRole, GroupNameRole, ChildCountRole };

    explicit ObsidiumInstanceTree(QSortFilterProxyModel* source, QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// The key of an instance is its id; of a group, "group/" and its name (instance ids are folder names).
    static QString groupKey(const QString& group) { return QStringLiteral("group/") + group; }

    QModelIndex indexOfKey(const QString& key) const;
    QModelIndex sourceIndex(const QModelIndex& index) const;
    QModelIndex fromSource(const QModelIndex& source) const;
    bool isGroup(const QModelIndex& index) const;
    QString key(const QModelIndex& index) const;

    /// Moves `key` (an instance or a group) into `group` ("" for the top level) before `before`
    /// (a key in that container; empty for its end). An instance changes group if it has to.
    void move(const QString& key, const QString& group, const QString& before);

   signals:
    /// Emitted after the rows were laid out again (a reset): expansion and selection are to be restored.
    void rebuilt();

   public slots:
    void scheduleRebuild();
    void rebuild();
    /// Lays the rows out now if the source changed since (it is done after the event otherwise).
    void flush()
    {
        if (m_rebuildQueued)
            rebuild();
    }

   private:
    struct Node {
        bool group = false;
        QString key;   // instance id, or group name
        QString name;  // the group's name
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
    };
    Node* node(const QModelIndex& index) const;
    QModelIndex indexOf(const Node* n) const;
    QStringList structureOf(const Node& root) const;
    void saveOrder() const;

    QSortFilterProxyModel* m_source;
    Node m_root;
    QHash<QString, int> m_sourceRows;  // instance id -> row in the source
    bool m_rebuildQueued = false;
};

/// One row: an instance (icon, name, gear) or a group header (arrow, name, count, gear).
class ObsidiumInstanceDelegate : public QStyledItemDelegate {
   public:
    explicit ObsidiumInstanceDelegate(ObsidiumInstanceList* list);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// The row's painted rectangle within the row `rect` (the gaps between rows, the indent of a group's instances).
    QRect body(const QRect& rect, const QModelIndex& index) const;
    /// Where the gear of a row is drawn.
    QRect gearRect(const QRect& rect, const QModelIndex& index) const;

   private:
    QRect nameRect(const QRect& rect, const QModelIndex& index) const;

    ObsidiumInstanceList* m_list;
};

/// Obsidium: the user's instances in the launcher's look (the "Пользовательские" tab): a plain
/// list, with groups that fold. Instances and groups are dragged up and down, and instances into
/// and out of groups. The classic instance view stays hidden behind it with the selection that
/// the rest of the window follows; the two are kept in step.
class ObsidiumInstanceList : public QTreeView {
    Q_OBJECT
   public:
    explicit ObsidiumInstanceList(QWidget* parent = nullptr);

    /// The classic view's model and selection.
    void setSource(QSortFilterProxyModel* source, QItemSelectionModel* sourceSelection);
    void setScale(qreal factor);
    qreal scale() const { return m_scale; }
    /// The row whose gear is under the mouse, for the delegate.
    QModelIndex hoveredGear() const { return m_hoveredGear; }
    ObsidiumInstanceTree* tree() const { return m_tree; }
    /// The first instance as the list shows them (the source model's index), for choosing one by default.
    QModelIndex firstInstance() const;

   signals:
    /// The gear of an instance was clicked, or it was right-clicked: show its actions at `globalPos`.
    void actionsRequested(const QPoint& globalPos);
    /// The same for a group.
    void groupActionsRequested(const QString& group, const QPoint& globalPos);
    /// An instance was double-clicked or Enter was pressed on it (`index` is the source model's).
    void instanceActivated(QModelIndex index);
    /// Files or links dropped on the list (modpacks to install).
    void droppedURLs(QList<QUrl> urls);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void drawBranches(QPainter*, const QRect&, const QModelIndex&) const override {}

   private:
    /// Where a dragged row goes: into `group` ("" for the top level) before `before` (empty: at its end).
    struct DropTarget {
        bool valid = false;
        QString group;
        QString before;
        QRect line;      // the indicator: a line between rows...
        QRect box;       // ...or a frame around the group it goes into
    };

    bool onGear(const QPoint& pos, QModelIndex* index = nullptr) const;
    DropTarget dropTargetAt(const QPoint& pos, const QString& dragged) const;
    QString nextSibling(const QModelIndex& index) const;
    void startRowDrag(const QModelIndex& index);
    void restoreState();
    void syncFromSource(const QModelIndex& sourceCurrent);
    void autoScroll(const QPoint& pos);

    ObsidiumInstanceDelegate* m_delegate;
    ObsidiumInstanceTree* m_tree = nullptr;
    QSortFilterProxyModel* m_source = nullptr;
    QItemSelectionModel* m_sourceSelection = nullptr;
    qreal m_scale = 1.0;
    QPersistentModelIndex m_hoveredGear;
    QPersistentModelIndex m_hovered;
    QPersistentModelIndex m_pressed;
    QPoint m_pressPos;
    bool m_syncing = false;
    bool m_restoring = false;
    DropTarget m_drop;
};
