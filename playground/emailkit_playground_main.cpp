#include <emailkit/emailkit.hpp>
#include <emailkit/imap_client.hpp>
#include <emailkit/log.hpp>
#include <iostream>

int main() {
  asio::io_context ctx;

  log_debug("debug message: {}", 123);
  log_info("info message: {}", 123);
  log_warning("warning message: {}", 123);
  log_error("error message: {}", 123);

  emailkit::emailkit_t emailkit{ctx};
  emailkit.async_method([](std::error_code ec) {
    if (ec) {
      std::cout << "error: emailkit_t::async_method done: " << ec.message()
                << "\n";
    } else {
      std::cout << "emailkit_t::async_method done\n";
    }
  });

  auto imap_client = emailkit::make_imap_client(ctx);
  const std::string gmail_imap_uri = "imap.gmail.com";
  imap_client->async_connect(gmail_imap_uri, 993, [](std::error_code ec) {
    log_debug("imap connected: {}", ec.message());
  });

  ctx.run();
}
