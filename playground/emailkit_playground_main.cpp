#include <emailkit/emailkit.hpp>
#include <iostream>

int main() {
  asio::io_context ctx;

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
