#include "connection.hpp"
#include <array>
#include <asio/write.hpp>
#include <emailkit/log.hpp>
#include "request_parser.hpp"

namespace emailkit::http_srv {

class connection_t_impl : public connection_t,
                          public std::enable_shared_from_this<connection_t_impl> {
   public:
    explicit connection_t_impl(asio::io_context& ctx, asio::ip::tcp::socket s, host_callbacks_t cbs)
        : m_ctx(ctx), m_socket(std::move(s)), m_cbs(std::move(cbs)) {}

    ~connection_t_impl() { log_debug("connection_t_impl"); }

    virtual void start() override {
        log_debug("starting communication on socket: {}",
                  m_socket.local_endpoint().address().to_string());
        do_read();
    }

    virtual std::error_code stop() override {
        log_debug("closing socket: {}", m_socket.local_endpoint().address().to_string());
        std::error_code ec;
        m_socket.close(ec);
        return ec;
    }

    void do_read() {
        // so we read request.
        m_socket.async_read_some(asio::buffer(m_buffer), [this_weak = weak_from_this()](
                                                             std::error_code ec,
                                                             size_t bytes_transfered) {
            if (ec) {
                if (ec != asio::error::eof) {
                    log_error("async_read_some failed: {}", ec);
                } else {
                    log_debug("async_read_some got eof");
                }

                // TODO: raise error up to host to indicate that this one has failed.
                // TODO: handle a case when connection was closed by server (eof).
                // operation aborted may be OK for us, in case it is we who initiated closing.
                return;
            }

            log_debug("read some bytes on socket: {}", bytes_transfered);

            auto this_ptr = this_weak.lock();
            if (!this_ptr) {
                log_debug("cannot lock self");
                return;
            }
            auto& this_ = *this_ptr;

            log_debug("parsing request..");
            auto [result, _] = this_.m_parser.parse(this_.m_request, this_.m_buffer.data(),
                                                    this_.m_buffer.data() + bytes_transfered);
            log_debug("parsing request DONE");
            switch (result) {
                case request_parser::good: {
                    // have request and can process
                    log_debug("have request and now can process: {}", this_.m_request);
                    this_.m_cbs.handle_request_cb(
                        this_.m_request, [this_weak = this_.weak_from_this()](
                                             std::error_code ec, emailkit::http_srv::reply reply) {
                            auto this_ptr = this_weak.lock();
                            if (!this_ptr) {
                                return;
                            }
                            auto& this_ = *this_ptr;

                            if (ec) {
                                log_error(
                                    "handler failed to process request, replying with 500: {}", ec);
                                this_.m_reply = reply::stock_reply(reply::internal_server_error);
                            } else {
                                log_debug("handler produced reply, writing into socket");
                                this_.m_reply = std::move(reply);
                            }
                            this_.do_write();
                        });

                    break;
                }
                case request_parser::bad: {
                    log_warning("returning bad request for connection: {}",
                                this_.m_socket.local_endpoint().address().to_string());
                    this_.m_reply = reply::stock_reply(reply::bad_request);
                    this_.do_write();
                    break;
                }
                default: {
                    log_debug("need more bytes, reading..");
                    // not enough data yet.
                    this_.do_read();
                }
            }
        });
    }

    void do_write() {
        log_debug("writing reply to: {}", m_socket.local_endpoint().address().to_string());
        asio::async_write(m_socket, m_reply.to_buffers(),
                          [this_weak = weak_from_this()](std::error_code ec, size_t) {
                              if (ec) {
                                  // TODO:
                                  // notify host that we have failed here.
                                  log_error("async write into socket failed: {}", ec.message());
                                  return;
                              }

                              auto this_ptr = this_weak.lock();
                              if (!this_ptr) {
                                  return;
                              }
                              auto& this_ = *this_ptr;

                              // we are basically 1.0 server with Connection: Close semantics.
                              // It is important to add this header to reply so tht remote side does
                              // not try to write socket being shutdown.
                              log_debug("initiate shutdown: {}",
                                        this_.m_socket.local_endpoint().address().to_string());
                              std::error_code ignored_ec;
                              this_.m_socket.shutdown(asio::ip::tcp::socket::shutdown_both,
                                                      ignored_ec);
                              log_debug("shutdown done: {}", ignored_ec);
                          });
    }

   private:
    asio::io_context& m_ctx;
    asio::ip::tcp::socket m_socket;
    host_callbacks_t m_cbs;
    std::array<char, 8192> m_buffer;
    emailkit::http_srv::request m_request;
    emailkit::http_srv::reply m_reply;
    emailkit::http_srv::request_parser m_parser;
};

std::shared_ptr<connection_t> make_connection(asio::io_context& ctx,
                                              asio::ip::tcp::socket s,
                                              host_callbacks_t cbs) {
    return std::make_shared<connection_t_impl>(ctx, std::move(s), std::move(cbs));
}

}  // namespace emailkit::http_srv
