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
    void create_folder_action_triggered();
    void expand_entire_tree() { expandRecursively(rootIndex()); }
    void tree_menu_changed(QMenu*);

   signals:
    void selected_folder_changed(QModelIndex curr, QModelIndex prev);
    void new_folder(const QModelIndex& parent);

   protected:
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override {
        qDebug("current changed");
        emit selected_folder_changed(current, previous);
    }

   private:
    QMenu* m_context_menu;
    QMenu* m_folder_item_context_menu;
    QMenu* m_contact_group_item_context_menu;
    QMenu* m_tree_menu = nullptr;
    QAction* m_add_folder_action;
    QAction* m_remove_folder_action;
    QAction* m_move_to_folder_action = nullptr;    

    QModelIndex m_clicked_index;
};
