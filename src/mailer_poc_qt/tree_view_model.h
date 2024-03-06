#pragma once

#include <QAbstractItemModel>

class TreeViewModel : public QAbstractItemModel {
    Q_OBJECT
   public:
    TreeViewModel(QObject* parent = nullptr);

   public:
    // QAbstractItemModel interface
   public:
    QModelIndex index(int row, int column, const QModelIndex& parent) const;
    QModelIndex parent(const QModelIndex& child) const;
    int rowCount(const QModelIndex& parent) const;
    int columnCount(const QModelIndex& parent) const;
    QVariant data(const QModelIndex& index, int role) const;
};
