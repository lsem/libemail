#pragma once
#include <asio/io_context.hpp>
#include <emailkit/global.hpp>
#include <memory>
#include <string>
#include "reply.hpp"
#include "request.hpp"

namespace emailkit::http_srv {

using handler_func_t = fu2::unique_function<void(const emailkit::http_srv::request& req,
                                                 async_callback<emailkit::http_srv::reply>)>;

class http_srv_t {
   public:
    virtual ~http_srv_t() = default;
    virtual std::error_code start() = 0;
    virtual std::error_code stop() = 0;
    virtual void register_handler(std::string method, std::string pattern, handler_func_t) = 0;
};

std::shared_ptr<http_srv_t> make_http_srv(asio::io_context& ctx,
                                          std::string host,
                                          std::string port);

}  // namespace emailkit::http_srv
