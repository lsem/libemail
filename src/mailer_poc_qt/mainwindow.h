#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QListView>
#include <QMainWindow>
#include <QSplitter>
#include <QTreeView>
#include <QVBoxLayout>

#include <mailer_poc.hpp>

#include <tree_view_model.h>

#include <thread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow(QWidget* parent, std::shared_ptr<mailer::MailerPOC> mailer_poc);
    ~MainWindow();

   private:
    Ui::MainWindow* ui;
    QTreeView* m_tree_view;
    QListView* m_list_view;
    QSplitter* m_spliter;

    TreeViewModel* m_tree_view_model;

    std::shared_ptr<mailer::MailerPOC> m_mailer_poc;
    std::thread m_mailer_eventloop_thread;
    asio::io_context m_ctx;
};
#endif  // MAINWINDOW_H
