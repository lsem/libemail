#include "list_view_model.h"

vector<string> collect_threads(mailer::MailerUIState::TreeNode* node) {
    if (!node) {
        return {};
    }
    vector<string> r;
    for (auto c : node->children) {
        if (c->ref) {
            r.emplace_back(c->ref->label);
        }
    }
    return r;
}

ListViewModel::ListViewModel(QObject* parent) : QAbstractListModel(parent) {}

int ListViewModel::rowCount(const QModelIndex& parent) const {
    return collect_threads(m_active_folder).size();
}
int ListViewModel::columnCount(const QModelIndex& parent) const {
    return 1;
}
QVariant ListViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return QVariant{};
    }
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }
    auto threads = collect_threads(m_active_folder);
    if (index.row() < threads.size()) {
        return threads[index.row()].c_str();
    } else {
        return QVariant{};
    }
}
