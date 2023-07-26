#include "imap_client.hpp"

#include <asio/ip/tcp.hpp>

namespace emailkit {

namespace {
class imap_client_impl_t : public imap_client_t, std::enable_shared_from_this<imap_client_impl_t> {
 public:
  explicit imap_client_impl_t(asio::io_context& ctx) : m_ctx(ctx) {}

  virtual void async_connect(std::string host, int port, async_callback<void> cb) override {
    log_debug("async_connect is working ..");

    asio::ip::tcp::resolver resolver{m_ctx};
    asio::ip::tcp::resolver::query query{host, std::to_string(port)};

    std::error_code ec;
    auto endpoints = resolver.resolve(query, ec);
    if (ec) {
      log_error("resolve failed: {}", ec.message());
      cb(ec);
      return;
    }
    for (auto x : endpoints) {
      log_info("ip address: {}", x.endpoint().address().to_string());
    }

    cb(std::error_code());
  }

 private:
  asio::io_context& m_ctx;
};

}  // namespace

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx) {
  return std::make_shared<imap_client_impl_t>(ctx);
}

}  // namespace emailkit