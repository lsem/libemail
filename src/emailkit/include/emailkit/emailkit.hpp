#pragma once
#include <emailkit/global.hpp>
#include <asio/io_context.hpp>
#include <async_kit/async_callback.hpp>
#include <memory>

#include <emailkit/log.hpp>

namespace emailkit {
class emailkit_t {
public:
  explicit emailkit_t(asio::io_context &ctx) : m_ctx(ctx) {}  

  void async_method(async_callback<void> cb) {
    log_debug("async_method starting...");
    m_ctx.post([cb = std::make_shared<decltype(cb)>(std::move(cb))]() mutable {
      log_debug("async_method DONE");
      (*cb)(std::error_code());
    });
  }

  asio::io_context &m_ctx;
};

} // namespace emailkit