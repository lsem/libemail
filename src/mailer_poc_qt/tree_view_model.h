#pragma once

#include <QAbstractItemModel>
#include <mailer_ui_state.hpp>

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

   public:
    // QAbstractItemModel interface

    void set_mailer_ui_state(mailer::MailerUIState* s) { m_mailer_ui_state = s; }

    void begin_reset() {
        qDebug("begin_reset!");
        beginResetModel();
    }
    void end_reset() {
        qDebug("end_reset!");
        endResetModel();
    }

    QModelIndex index(int row, int column, const QModelIndex& parent) const;
    QModelIndex parent(const QModelIndex& child) const;
    int rowCount(const QModelIndex& parent) const;
    int columnCount(const QModelIndex& parent) const;
    QVariant data(const QModelIndex& index, int role) const;

   private:
    mailer::MailerUIState* m_mailer_ui_state = nullptr;
};
