#include "tree_view_model.h"

#include <mailer_ui_state.hpp>

// https://doc.qt.io/qt-5/qtwidgets-itemviews-simpletreemodel-example.html
TreeViewModel::TreeViewModel(QObject* parent) : QAbstractItemModel(parent) {}

QModelIndex TreeViewModel::index(int row, int column, const QModelIndex& parent) const {
    assert(m_mailer_ui_state);
    assert(column == 0);

    mailer::MailerUIState::TreeNode* parent_node = nullptr;

    if (!parent.isValid()) {
        qWarning("PARRENT IS INVALID: ASSINGING ROOT AS PARENT");
        parent_node = &m_mailer_ui_state->m_root;
    } else {
        parent_node = static_cast<mailer::MailerUIState::TreeNode*>(parent.internalPointer());
    }

    qDebug("parent node: %p", (void*)parent_node);

    if (row < parent_node->children.size()) {
        qDebug("creating index for node %p", (void*)parent_node->children.at(row));
        return createIndex(row, column, parent_node->children.at(row));
    } else {
        qWarning("REQUESTED INVALID INDEX");
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
                //  assert(false);
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
        qWarning("PARRENT IS INVALID: ASSINGING ROOT AS PARENT");
        parent_node = &m_mailer_ui_state->m_root;
        assert(parent_node);
    } else {
        parent_node = static_cast<mailer::MailerUIState::TreeNode*>(parent.internalPointer());
        assert(parent_node);
    }

    qDebug("accessing node %p", (void*)parent_node);

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
        if (node->ref) {
            return node->ref->label.c_str();
        } else {
            return node->label.c_str();
        }
    }

    return QVariant{};
}
