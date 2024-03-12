#pragma once

#include <QAbstractListModel>
#include <mailer_ui_state.hpp>

class ListViewModel : public QAbstractListModel {
    Q_OBJECT
   public:
    ListViewModel(QObject* parent = nullptr);

    void set_mailer_ui_state(mailer::MailerUIState* s) { m_mailer_ui_state = s; }
    void set_active_folder(mailer::MailerUIState::TreeNode* node) {
        begin_reset();
        log_debug("new active folder");
        m_active_folder = node;
        end_reset();
    }

    void begin_reset() {
        qDebug("begin_reset!");
        beginResetModel();
    }
    void end_reset() {
        qDebug("end_reset!");
        endResetModel();
    }

   public:
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

   private:
    mailer::MailerUIState* m_mailer_ui_state = nullptr;
    mailer::MailerUIState::TreeNode* m_active_folder = nullptr;
};
