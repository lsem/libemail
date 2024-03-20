#include "tree_view_model.h"

#include <QDebug>
#include <QMimeData>
#include <mailer_ui_state.hpp>

#include <sstream>

constexpr auto DRAGGED_INDEXES_MIME_TYPE = "application/dragged-indexes.list";

// https://doc.qt.io/qt-5/qtwidgets-itemviews-simpletreemodel-example.html
// https://doc.qt.io/qt-6/qtwidgets-itemviews-editabletreemodel-example.html
// https://forum.qt.io/topic/87273/how-can-i-select-a-child-node-in-a-qtreeview
// https://stackoverflow.com/questions/54146553/how-to-get-the-index-from-an-item-in-the-qtreeview
TreeViewModel::TreeViewModel(QObject* parent) : QAbstractItemModel(parent) {}

void TreeViewModel::initiate_rename(mailer::TreeNode* node) {
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

QModelIndex TreeViewModel::encode_model_index(mailer::TreeNode* node) const {
    const int row_index = node->child_index();
    if (row_index < 0) {
        qWarning("could not find row index for node: %p", node);
        return QModelIndex{};
    }
    return createIndex(row_index, 0, node);
}

mailer::TreeNode* TreeViewModel::decode_model_index(const QModelIndex& index) const {
    return static_cast<mailer::TreeNode*>(index.internalPointer());
}

QModelIndex TreeViewModel::index(int row, int column, const QModelIndex& parent) const {
    assert(m_mailer_ui_state);
    assert(column == 0);

    mailer::TreeNode* parent_node = nullptr;

    if (!parent.isValid()) {
        parent_node = &m_mailer_ui_state->m_root;
    } else {
        parent_node = decode_model_index(parent);
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

    auto child_node = decode_model_index(child);
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

    mailer::TreeNode* parent_node = nullptr;

    if (!parent.isValid()) {
        parent_node = m_mailer_ui_state->tree_root();
        assert(parent_node);
    } else {
        parent_node = decode_model_index(parent);
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
        auto node = decode_model_index(index);
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
    if (new_value.empty()) {
        return false;
    }
    auto node = decode_model_index(index);
    node->label = std::move(new_value);
    m_mailer_ui_state->notify_change();

    return true;
}

Qt::ItemFlags TreeViewModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable |
           Qt::ItemIsSelectable | QAbstractItemModel::flags(index);
}

Qt::DropActions TreeViewModel::supportedDropActions() const {
    return Qt::MoveAction;
}

QMimeData* TreeViewModel::mimeData(const QModelIndexList& indexes) const {
    auto mime_data = new QMimeData();
    qDebug() << "mimeData, indexes:" << indexes;

    std::string indices_list;
    int key = 0;
    for (auto idx : indexes) {
        auto key_str = std::to_string(key);
        m_dragged_items[key_str] = idx;
        indices_list += key_str;
        indices_list += ",";
        key++;
    }

    if (!indices_list.empty()) {
        indices_list.resize(indices_list.size() - 1);
    }

    mime_data->setData(DRAGGED_INDEXES_MIME_TYPE, QByteArray(indices_list.c_str()));
    return mime_data;
}

bool TreeViewModel::dropMimeData(const QMimeData* data,
                                 Qt::DropAction action,
                                 int row,
                                 int column,
                                 const QModelIndex& parent) {
    qDebug() << "dropMimeData: " << data;

    if (action != Qt::MoveAction) {
        qDebug() << "unsupported drop action, ignoring";
        return false;
    }

    if (!data->hasFormat(DRAGGED_INDEXES_MIME_TYPE)) {
        qDebug() << "unsupported drop format";
        return false;
    }

    std::vector<mailer::TreeNode*> source_nodes;

    const auto byte_array = data->data(DRAGGED_INDEXES_MIME_TYPE);
    const auto keys = QString::fromUtf8(byte_array).split(",", Qt::SkipEmptyParts);
    for (auto& k : keys) {
        if (auto it = m_dragged_items.find(k.toStdString()); it != m_dragged_items.end()) {
            qDebug() << "Dropped index: " << it->second;
            source_nodes.emplace_back(decode_model_index(it->second));
        } else {
            qDebug() << "ERROR: no index in registry for key " << k;
        }
    }

    // We ignore row and column for now but it makes kind of sense.
    auto drop_node = decode_model_index(parent);
    qDebug() << "Drop taget: row: " << row << ", column: " << column << ", parent: " << parent
             << ", label: " << drop_node->label.c_str();

    // TODO: once we make top level folder corresponding to INBOX level we can get rud of this check
    // for non-root.
    if (!drop_node->is_folder_node() && drop_node != m_mailer_ui_state->tree_root()) {
        qDebug() << "dropping into non-folder node is not allowed";
        return false;
    }

    m_dragged_items.clear();

    // Theoretically we should not be doing this but should modify model directly since we are the
    // model here and that is why entire drag and drop handling happens in Qt here but not in
    // QTreeView.
    if (row == -1) {
        emit items_move_requested(source_nodes, drop_node, std::nullopt);
    } else {
        emit items_move_requested(source_nodes, drop_node, row);
    }

    return true;
}

QStringList TreeViewModel::mimeTypes() const {
    QStringList types;
    types << DRAGGED_INDEXES_MIME_TYPE;
    return types;
}
