#include "list_view_model.h"

vector<string> collect_threads(mailer::MailerUIState::TreeNode* node) {
    if (!node) {
        return {};
    }
    vector<string> r;
    for (auto c : node->threads_refs) {
        r.emplace_back(c.label);
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

    const auto& threads = m_active_folder->threads_refs;
    if (index.row() > threads.size()) {
        return QVariant{};
    }

    auto thread_ref = threads[index.row()];

    const std::string label = fmt::format(
        "{} (emails: {}{})", (thread_ref.label.empty() ? "<No-Subject>" : thread_ref.label),
        thread_ref.emails_count,
        thread_ref.attachments_count > 0
            ? fmt::format(", attachments: {}", thread_ref.attachments_count)
            : "");

    return label.c_str();
}
