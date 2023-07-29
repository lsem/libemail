#pragma once
#include <asio/io_context.hpp>
#include <emailkit/global.hpp>
#include <memory>
#include <string>

namespace emailkit::http_srv {
class http_srv_t {
   public:
    virtual ~http_srv_t() = default;
    virtual bool start() = 0;
    virtual void register_handler(std::string method, async_callback<std::string> cb) = 0;
};

std::shared_ptr<http_srv_t> make_http_srv(asio::io_context& ctx,
                                          std::string host,
                                          std::string port);

}  // namespace emailkit::http_srv
