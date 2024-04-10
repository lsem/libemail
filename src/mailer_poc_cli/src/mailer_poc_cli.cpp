#include <iostream>
#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>

#include <mailer_poc.hpp>

int main() {
    auto mailer_poc = mailer::make_mailer_poc();
    if (!mailer_poc) {
        log_error("failed creating mailer poc");
        return 1;
    }

    mailer_poc->async_run([](std::error_code ec) {
        if (ec) {
            log_error("failed running the app: {}", ec);
            std::exit(1);
            return;
        }
        log_info("mailer poc is running..");
    });

    log_info("starting mailer poc event loop");
    mailer_poc->start_working_thread();

    // TODO: now we need to wait somehow...
    asio::io_context ctx;
    asio::signal_set sset{ctx, SIGINT, SIGTERM};
    sset.async_wait([&](std::error_code ec, int signal) {
        // Now this could be be synchronized with MailerPOC's internal context by employing methold
        // like run_in_event_loop().
        mailer_poc->stop_working_thread();
    });
}
