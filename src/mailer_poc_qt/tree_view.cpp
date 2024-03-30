#include "tree_view.h"

#include <QDebug>

// https://forum.qt.io/topic/77668/qtreeview-hide-controls-for-expanding-and-collapsing-specific-items/3
// https://stackoverflow.com/questions/16018974/qtreeview-remove-decoration-expand-button-for-all-items
TreeView::TreeView(QWidget* parent) : QTreeView(parent) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setHeaderHidden(true);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this,
            SLOT(on_context_menu_requested(const QPoint&)));
    m_context_menu = new QMenu(this);
    m_add_folder_action = new QAction("Add folder", this);
    m_context_menu->addAction(m_add_folder_action);

    m_folder_item_context_menu = new QMenu(this);
    m_contact_group_item_context_menu = new QMenu(this);

    connect(m_add_folder_action, SIGNAL(triggered()), this, SLOT(create_folder_action_triggered()));

    // I dislike how Qt does autoexoansion and don't know how to control it so I disable expansion
    // and expandRecursively after data changes to resotre always-expand-state.
    setItemsExpandable(false);
    setStyleSheet("QTreeView::branch {  border-image: url(none.png); }");
}

void TreeView::create_folder_action_triggered() {
    qDebug("Emitting signal");
    emit new_folder(m_clicked_index);
}

void TreeView::tree_menu_changed(QMenu* menu) {
    // TODO: delete previous menu?
    m_tree_menu = menu;

    if (m_move_to_folder_action) {
        m_context_menu->removeAction(m_move_to_folder_action);
    }

    m_move_to_folder_action = m_context_menu->addMenu(m_tree_menu);
}

void TreeView::on_context_menu_requested(const QPoint& pt) {
    auto index = indexAt(pt);
    m_clicked_index = index;
    if (index.isValid()) {
        qDebug("clicked at valid index");
        m_context_menu->exec(this->viewport()->mapToGlobal(pt));
    } else {
        qDebug("clicked at INVALID index");
        m_context_menu->exec(this->viewport()->mapToGlobal(pt));
    }
}

void TreeView::prompt_rename(QModelIndex index) {
    selectionModel()->clearSelection();
    selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    edit(index);
}
