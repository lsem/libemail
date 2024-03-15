#pragma once

#include <QMenu>
#include <QTreeView>

class TreeView : public QTreeView {
    Q_OBJECT
   public:
    explicit TreeView(QWidget* parent = nullptr);

   public slots:
    void on_context_menu_requested(const QPoint&);
    void prompt_rename(QModelIndex index);

   signals:
    void selected_folder_changed(QModelIndex curr, QModelIndex prev);
    void new_folder();

   protected:
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override {
        qDebug("current changed");
        emit selected_folder_changed(current, previous);
    }

   private:
    QMenu* m_context_menu;
    QAction* m_add_folder_action;
};
