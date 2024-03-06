#include "mainwindow.h"
#include "./ui_mainwindow.h"

// https://www.youtube.com/watch?v=Hbh9BMtgE50&ab_channel=MacDigia
MainWindow::MainWindow(QWidget* parent, std::shared_ptr<mailer::MailerPOC> mailer_poc)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_mailer_poc{mailer_poc} {
    ui->setupUi(this);

    m_tree_view_model = new TreeViewModel(this);

    m_tree_view = new QTreeView(this);
    m_tree_view->setModel(m_tree_view_model);

    m_list_view = new QListView(this);
    m_spliter = new QSplitter(this);

    auto layout = new QVBoxLayout;

    m_spliter->addWidget(m_tree_view);
    m_spliter->addWidget(m_list_view);
    m_spliter->setOrientation(Qt::Horizontal);

    layout->addWidget(m_spliter);

    auto widget = new QWidget(this);
    widget->setLayout(layout);

    setCentralWidget(widget);
}

MainWindow::~MainWindow() {
    delete ui;
}
