#include "mainwindow.h"

#include <QApplication>

#include <mailer_poc.hpp>
#include <thread>

int main(int argc, char* argv[]) {
    asio::io_context ctx;

    std::function<void(std::string)> launch_browser_fn;

    auto mailer_poc = mailer::make_mailer_poc(ctx);

    std::thread mailer_poc_eventloop_th{[&] {
        asio::io_context::work w{ctx};
        log_info("starting mailer eventloop");
        ctx.run();
        log_info("stopping mailer eventloop");
    }};

    QApplication a(argc, argv);
    MainWindow w{nullptr, mailer_poc};

    mailer_poc->set_callbacks_if(&w);

    mailer_poc->async_run([](std::error_code ec) {
        if (ec) {
            log_error("failed running mailer poc: {}", ec);
            return;
        }

        log_info("mailer poc instance started, starting main window");
    });

    w.show();

    int code = a.exec();

    ctx.stop();
    if (mailer_poc_eventloop_th.joinable()) {
        mailer_poc_eventloop_th.join();
    }

    return code;
}
