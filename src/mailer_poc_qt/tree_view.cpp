#include "tree_view.h"

TreeView::TreeView(QWidget* parent) : QTreeView(parent) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setHeaderHidden(true);
}
