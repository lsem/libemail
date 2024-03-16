#include "tree_view.h"

#include <QDebug>

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

    connect(m_add_folder_action, SIGNAL(triggered()), this, SLOT(create_folder_action_triggered()));
}

void TreeView::create_folder_action_triggered() {
    qDebug("Emitting signal");
    emit new_folder(m_clicked_index);
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

    // for some reason, without this we have selection and editing work but it works on
    // collapsed node.
    auto x = index;
    while (x.parent().isValid() && x.parent().parent().isValid()) {
        x = x.parent();
    }
    expandRecursively(x);

    selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    edit(index);
}
