#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QEvent>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QSplitter>
#include <QStackedLayout>
#include <QStackedWidget>
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

class DispatchUILambdaEvent : public QEvent {
   public:
    std::function<void()> fn;

    explicit DispatchUILambdaEvent(std::function<void()> fn)
        : QEvent(QEvent::User), fn(std::move(fn)) {}

    static void dispatch_to(QObject* dest, std::function<void()> fn);
};

class MainWindow : public QMainWindow, public mailer::MailerPOCCallbacks {
    Q_OBJECT

   public:
    MainWindow(QWidget* parent, std::shared_ptr<mailer::MailerPOC> mailer_poc);
    ~MainWindow();
   public slots:
    void login_clicked();

   public:  // mailer::MailerPOCCallbacks
    void dispatch(std::function<void()> fn);

    void auth_initiated(std::string uri) override;
    void auth_done(std::error_code) override;
    void tree_model_changed() override;
    void tree_about_to_change() override;
    void update_state(std::function<void()> fn) override;

   public:
    bool event(QEvent* ev) override;

   private:
    Ui::MainWindow* ui;
    QTreeView* m_tree_view;
    QListView* m_list_view;
    QSplitter* m_spliter;
    QLabel* m_login_label;
    QWidget* m_mailer_ui_widget;
    QWidget* m_login_widget;
    TreeViewModel* m_tree_view_model;
    QAction* m_login_action;
    QMenu* m_actions_menu;

    QStackedWidget* m_stacked_widget;

    std::shared_ptr<mailer::MailerPOC> m_mailer_poc;
    std::thread m_mailer_eventloop_thread;
    asio::io_context m_ctx;
};
#endif  // MAINWINDOW_H
