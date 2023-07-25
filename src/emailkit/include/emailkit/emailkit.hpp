#pragma once
#include <asio/io_context.hpp>
#include <async_kit/async_callback.hpp>
#include <memory>

namespace emailkit {
class emailkit_t {
public:
  explicit emailkit_t(asio::io_context &ctx) : m_ctx(ctx) {}

  void async_method(acallback<void> cb) {
    // cb(std::error_code());
    m_ctx.post([cb = std::make_shared<decltype(cb)>(std::move(cb))]() mutable {
      (*cb)(std::error_code());
    });
  }

  asio::io_context &m_ctx;
};
} // namespace emailkit