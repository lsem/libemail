#pragma once
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <emailkit/global.hpp>
#include <memory>

#include "reply.hpp"
#include "request.hpp"

namespace emailkit::http_srv {

class connection_t {
   public:
    virtual ~connection_t() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

// interface for calling back host
using handle_request_cb_t = fu2::function<void(const emailkit::http_srv::request&,
                                               async_callback<emailkit::http_srv::reply> cb)>;
using connection_failed_cb_t = fu2::function<void()>;
struct host_callbacks_t {
    handle_request_cb_t handle_request_cb;
    connection_failed_cb_t connection_failed_cb;
};

std::shared_ptr<connection_t> make_connection(asio::io_context& ctx,
                                              asio::ip::tcp::socket s,
                                              host_callbacks_t);

}  // namespace emailkit::http_srv