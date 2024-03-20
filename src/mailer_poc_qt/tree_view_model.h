#pragma once

#include <QAbstractItemModel>
#include <mailer_ui_state.hpp>
#include <map>

// Here is a simpler tree representation of our mailer_ui_state.
struct TreeItem {
   public:
    TreeItem* parent = nullptr;
    vector<TreeItem*> children;
    QVariant data;
};

class TreeViewModel : public QAbstractItemModel {
    Q_OBJECT
   public:
    TreeViewModel(QObject* parent = nullptr);
    void set_mailer_ui_state(mailer::MailerUIState* s) { m_mailer_ui_state = s; }
    void initiate_rename(mailer::TreeNode* node);
    QModelIndex encode_model_index(mailer::TreeNode* node) const;
    mailer::TreeNode* decode_model_index(const QModelIndex&) const;

   signals:
    void items_move_requested(std::vector<mailer::TreeNode*> source_nodes,
                              mailer::TreeNode* destination,
                              std::optional<size_t>);

   public:
    void begin_reset() { beginResetModel(); }
    void end_reset() { endResetModel(); }

   public:
    // QAbstractItemModel interface

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    Qt::DropActions supportedDropActions() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data,
                      Qt::DropAction action,
                      int row,
                      int column,
                      const QModelIndex& parent) override;
    QStringList mimeTypes() const override;

   private:
    mailer::MailerUIState* m_mailer_ui_state = nullptr;
    mutable std::map<std::string, QModelIndex> m_dragged_items;
};
