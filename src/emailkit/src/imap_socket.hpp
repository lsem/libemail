#pragma once
#include <asio/io_context.hpp>
#include <emailkit/global.hpp>
#include <emailkit/log.hpp>
#include <memory>

namespace emailkit {

namespace imap_socket_opts {
    struct dump_stream_to_file {};
}

// https://datatracker.ietf.org/doc/html/rfc3501
class imap_socket_t {
   public:
    virtual ~imap_socket_t() = default;

    // todo: context with deadline?
    virtual void async_connect(std::string host, std::string port, async_callback<void> cb) = 0;
    virtual void async_receive_line(async_callback<std::string> cb) = 0;
    virtual void async_send_command(std::string command, async_callback<void> cb) = 0;

    virtual void set_option(imap_socket_opts::dump_stream_to_file ) = 0;
};

std::shared_ptr<imap_socket_t> make_imap_socket(asio::io_context& ctx);

void async_keep_receiving_lines_until(std::weak_ptr<imap_socket_t> socket_ptr,
                                      fu2::function<std::error_code(const std::string& l)> p,
                                      async_callback<void> cb);

}  // namespace emailkit