#pragma once
#include <asio/io_context.hpp>
#include <async_kit/async_callback.hpp>
#include <memory>

using lsem::async_kit::async_callback;

namespace emailkit {
class emailkit_t {
public:
  explicit emailkit_t(asio::io_context &ctx) : m_ctx(ctx) {}

  void async_method(async_callback<void> cb) {
    m_ctx.post([cb = std::make_shared<decltype(cb)>(std::move(cb))]() mutable {
      (*cb)(std::error_code());
    });
  }

  asio::io_context &m_ctx;
};

class imap_client_t {
public:
  explicit imap_client_t(asio::io_context &ctx);
  virtual ~imap_client_t() = default;
  virtual void async_connect(std::string uri, async_callback<void> cb) = 0;

  private:
  asio::io_context &m_ctx;
};

} // namespace emailkit