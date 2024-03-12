#pragma once

#include <QTreeView>

class TreeView : public QTreeView {
    Q_OBJECT
   public:
    explicit TreeView(QWidget* parent = nullptr);

   signals:
    void selected_folder_changed(QModelIndex curr, QModelIndex prev);

   protected:
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override {
        qDebug("current changed");
        emit selected_folder_changed(current, previous);
    }
};
