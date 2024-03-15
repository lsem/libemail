#include "tree_view_model.h"

#include <QDebug>
#include <mailer_ui_state.hpp>

// https://doc.qt.io/qt-5/qtwidgets-itemviews-simpletreemodel-example.html
// https://doc.qt.io/qt-6/qtwidgets-itemviews-editabletreemodel-example.html
// https://forum.qt.io/topic/87273/how-can-i-select-a-child-node-in-a-qtreeview
// https://stackoverflow.com/questions/54146553/how-to-get-the-index-from-an-item-in-the-qtreeview
TreeViewModel::TreeViewModel(QObject* parent) : QAbstractItemModel(parent) {
    //    connect(this, &TreeViewModel::rowsInserted, this, TreeViewModel::on_rows_inserted);
}

void TreeViewModel::on_rows_inserted(const QModelIndex& parent, int first, int last) {}

void TreeViewModel::initiate_rename(mailer::MailerUIState::TreeNode* node) {
    qDebug("requested renaming node by node id");

    // We can create QModelIndex on our own by having a node.
    // This can be done because we know our parent.
    const int row_index = node->child_index();
    if (row_index < 0) {
        qDebug("could not find row index for node: %p", node);
        // TODO: so what to do next?
        return;
    }

    auto index = createIndex(row_index, 0, node);

    // The node with given index may not exist yet, so we shoudl probably wait first (here or to
    // make the caller doing this).
}

QModelIndex TreeViewModel::encode_model_index(mailer::MailerUIState::TreeNode* node) const {
    const int row_index = node->child_index();
    if (row_index < 0) {
        qWarning("could not find row index for node: %p", node);
        return QModelIndex{};
    }
    return createIndex(row_index, 0, node);
}

QModelIndex TreeViewModel::index(int row, int column, const QModelIndex& parent) const {
    assert(m_mailer_ui_state);
    assert(column == 0);

    mailer::MailerUIState::TreeNode* parent_node = nullptr;

    if (!parent.isValid()) {
        parent_node = &m_mailer_ui_state->m_root;
    } else {
        parent_node = static_cast<mailer::MailerUIState::TreeNode*>(parent.internalPointer());
    }

    if (row < parent_node->children.size()) {
        return createIndex(row, column, parent_node->children.at(row));
    } else {
        return QModelIndex{};
    }
}

QModelIndex TreeViewModel::parent(const QModelIndex& child) const {
    assert(m_mailer_ui_state);

    if (!child.isValid()) {
        return QModelIndex{};
    }

    auto child_node = static_cast<mailer::MailerUIState::TreeNode*>(child.internalPointer());
    if (!child_node) {
        return QModelIndex{};
    }

    auto parent = child_node->parent;

    if (!parent) {
        return QModelIndex{};
    }
    if (parent->parent) {
        for (size_t i = 0; i < parent->parent->children.size(); ++i) {
            if (parent->parent->children[i] == parent) {
                return createIndex(i, 0, parent);
            }
        }
    } else {
        // parent is a root.
        return createIndex(0, 0, parent);
    }

    qWarning("Returning invalid qmodelindex because indexOf failed");
    return QModelIndex{};
}

int TreeViewModel::rowCount(const QModelIndex& parent) const {
    assert(m_mailer_ui_state);

    if (parent.column() > 0) {
        return 0;
    }

    mailer::MailerUIState::TreeNode* parent_node = nullptr;

    if (!parent.isValid()) {
        parent_node = &m_mailer_ui_state->m_root;
        assert(parent_node);
    } else {
        parent_node = static_cast<mailer::MailerUIState::TreeNode*>(parent.internalPointer());
        assert(parent_node);
    }

    return parent_node->children.size();
}

int TreeViewModel::columnCount(const QModelIndex& parent) const {
    return 1;
}

QVariant TreeViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return QVariant{};
    }

    if (role == Qt::DisplayRole) {
        auto node = static_cast<mailer::MailerUIState::TreeNode*>(index.internalPointer());
        assert(node);

        return node->label.c_str();
    }

    return QVariant{};
}

bool TreeViewModel::setData(const QModelIndex& index,
                            const QVariant& value,
                            int role /* = Qt::EditRole*/) {
    if (!index.isValid()) {
        qWarning() << "invalid index" << value;
        return false;
    }
    std::string new_value = value.toString().toUtf8().data();
    auto node = static_cast<mailer::MailerUIState::TreeNode*>(index.internalPointer());
    node->label = new_value;
    return true;
}

Qt::ItemFlags TreeViewModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEditable | QAbstractItemModel::flags(index);
}
