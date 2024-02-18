#include <iostream>

#include <mailer_poc.hpp>

int main() {
    asio::io_context ctx;

    auto mailer_poc = mailer::make_mailer_poc(ctx);
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
    ctx.run();
    log_info("mailer poc event loop has finished");
}
