#pragma once
#include <asio/io_context.hpp>
#include <emailkit/global.hpp>
#include <emailkit/log.hpp>
#include <memory>

namespace emailkit {

class imap_client_t {
public:
  virtual ~imap_client_t() = default;
  virtual void async_connect(std::string host, int port,
                             async_callback<void> cb) = 0;
};

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context &ctx);

} // namespace emailkit