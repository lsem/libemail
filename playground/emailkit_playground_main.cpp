#include <emailkit/emailkit.hpp>
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

  ctx.run();
}
