#include "tree_view_model.h"

TreeViewModel::TreeViewModel(QObject* parent) : QAbstractItemModel(parent) {}
QModelIndex TreeViewModel::index(int row, int column, const QModelIndex& parent) const {
    return QModelIndex{};
}

QModelIndex TreeViewModel::parent(const QModelIndex& child) const {
    return QModelIndex{};
}
int TreeViewModel::rowCount(const QModelIndex& parent) const {
    return 0;
}
int TreeViewModel::columnCount(const QModelIndex& parent) const {
    return 0;
}
QVariant TreeViewModel::data(const QModelIndex& index, int role) const {
    return QVariant{};
}
