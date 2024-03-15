#include "tree_view.h"

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

    connect(m_add_folder_action, SIGNAL(triggered()), this, SIGNAL(new_folder()));
}

void TreeView::on_context_menu_requested(const QPoint& pt) {
    auto index = indexAt(pt);
    if (index.isValid()) {
        qDebug("clicked at valid index");
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
