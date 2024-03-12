#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>

namespace {
bool launch_system_browser(std::string uri) {
    // https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html

#if defined(__linux__)
    // TODO: see more variants if this does not work
    // https://www.baeldung.com/linux/open-url-in-default-browser
    return std::system(fmt::format("xdg-open {} > /dev/null 2>&1", uri).c_str()) == 0;

#elif defined(__APPLE__)
    return std::system(fmt::format("open {}", uri).c_str()) == 0;
#elif defined(__WIN32)
    return std::system(fmt::format("start {}", uri).c_str()) == 0;
#endif
}

}  // namespace

void DispatchUILambdaEvent::dispatch_to(QObject* dest, std::function<void()> fn) {
    QCoreApplication::postEvent(dest, new DispatchUILambdaEvent(std::move(fn)));
}

// https://www.youtube.com/watch?v=Hbh9BMtgE50&ab_channel=MacDigia
// https://www.youtube.com/watch?v=6LweUh9kVB8&ab_channel=%D0%9B%D0%B5%D0%B2%D0%90%D0%BB%D0%B5%D0%BA%D1%81%D0%B5%D0%B5%D0%B2%D1%81%D0%BA%D0%B8%D0%B9
// https://stackoverflow.com/questions/26876151/qt-add-function-call-to-event-loop-from-other-thread
// https://stackoverflow.com/questions/9485339/design-pattern-qt-model-view-and-multiple-threads
// https://stackoverflow.com/questions/9485339/design-pattern-qt-model-view-and-multiple-threads
MainWindow::MainWindow(QWidget* parent, std::shared_ptr<mailer::MailerPOC> mailer_poc)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_mailer_poc{mailer_poc} {
    ui->setupUi(this);

    m_tree_view_model = new TreeViewModel(this);
    m_tree_view = new TreeView(this);
    m_tree_view_model->set_mailer_ui_state(m_mailer_poc->get_ui_model());
    m_tree_view->setModel(m_tree_view_model);

    connect(m_tree_view, &TreeView::selected_folder_changed, this,
            &MainWindow::selected_folder_changed);

    m_list_view_model = new ListViewModel(this);
    m_list_view_model->set_mailer_ui_state(m_mailer_poc->get_ui_model());
    m_list_view = new ListView(this);
    m_list_view->setModel(m_list_view_model);

    // MailerUI widget
    auto main_window_layout = new QVBoxLayout;

    m_spliter = new QSplitter(this);
    m_spliter->addWidget(m_tree_view);
    m_spliter->addWidget(m_list_view);
    m_spliter->setOrientation(Qt::Horizontal);
    m_spliter->setSizes(QList<int>({300, 500}));
    main_window_layout->addWidget(m_spliter);
    main_window_layout->setContentsMargins(0, 0, 0, 0);
    m_mailer_ui_widget = new QWidget(this);
    m_mailer_ui_widget->setLayout(main_window_layout);

    // Login widget
    auto login_layout = new QVBoxLayout;
    m_login_label = new QLabel(this);
    m_login_label->setText("Login in extenral browser");
    m_login_label->setAlignment(Qt::AlignCenter);
    login_layout->addWidget(m_login_label);
    m_login_widget = new QWidget(this);
    m_login_widget->setLayout(login_layout);

    // Central widget is a stack which has several possible "screens": login screen, main screen and
    // possiby more.
    m_stacked_widget = new QStackedWidget(this);
    m_stacked_widget->addWidget(m_mailer_ui_widget);
    m_stacked_widget->addWidget(m_login_widget);

    setCentralWidget(m_stacked_widget);

    m_login_action = new QAction("&Login", this);
    connect(m_login_action, &QAction::triggered, this, &MainWindow::login_clicked);
    m_actions_menu = menuBar()->addMenu("&Actions");
    m_actions_menu->addAction(m_login_action);
}

void MainWindow::login_clicked() {
    // TODO: this should be asked from mailer_poc application itself.
    //    login_requested
}

void MainWindow::selected_folder_changed(const QModelIndex& curr, const QModelIndex& prev) {
    qDebug("selected folder changed: %d", curr.row());
    // TODO: how we are supposed to handle all of this in the corresponding thread?
    auto* selected_node = static_cast<mailer::MailerUIState::TreeNode*>(curr.internalPointer());
    assert(selected_node);
    // TODO: what thread it should be?
    m_mailer_poc->selected_folder_changed(selected_node);
    m_list_view_model->set_active_folder(selected_node);
}

void MainWindow::auth_initiated(std::string uri) {
    dispatch([this, uri] {
        m_stacked_widget->setCurrentIndex(1);
        // TODO: login page is going to be a static thing that writes instuctioins to how login via
        // browser.
        launch_system_browser(uri);
    });
}

void MainWindow::auth_done(std::error_code ec) {
    // TODO: somehow I need to deload web engine
    log_debug("Auth done: {}", ec);
    dispatch([this] { m_stacked_widget->setCurrentIndex(0); });
}

void MainWindow::dispatch(std::function<void()> fn) {
    DispatchUILambdaEvent::dispatch_to(this, std::move(fn));
}

void MainWindow::tree_about_to_change() {
    dispatch([this] {
        m_tree_view_model->begin_reset();
        m_list_view_model->begin_reset();
    });
}

void MainWindow::tree_model_changed() {
    log_debug("tree model changed");
    dispatch([this] {
        m_tree_view_model->end_reset();
        m_list_view_model->end_reset();
    });
}

void MainWindow::update_state(std::function<void()> fn) {
    dispatch([fn = std::move(fn)] {
        log_debug("calling update state fn from UI thead -- begin");
        fn();
        log_debug("calling update state fn from UI thead -- endx");
    });
}

bool MainWindow::event(QEvent* ev) {
    if (ev->type() == QEvent::User) {
        log_debug("received user event, running custom func");
        static_cast<DispatchUILambdaEvent*>(ev)->fn();
        return true;
    }
    return QWidget::event(ev);
}

MainWindow::~MainWindow() {
    delete ui;
}
