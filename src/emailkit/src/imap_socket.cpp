#include "imap_socket.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/ssl.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <fstream>
#include <system_error>

#include "utils.hpp"

#define RAISE_CB_ON_ERROR(ec)                     \
    do {                                          \
        if (ec) {                                 \
            log_error("error: {}", ec.message()); \
            cb(ec);                               \
            return;                               \
        }                                         \
    } while (false)

namespace emailkit {

namespace {
class imap_client_impl_t : public imap_socket_t, std::enable_shared_from_this<imap_client_impl_t> {
   public:
    explicit imap_client_impl_t(asio::io_context& ctx)
        : m_ctx(ctx), m_ssl_ctx(asio::ssl::context::sslv23), m_socket(m_ctx, m_ssl_ctx) {}

    virtual void async_connect(std::string host,
                               std::string port,
                               async_callback<void> cb) override {
        log_debug("async_connect is working ..");

        // TODO: async?
        asio::ip::tcp::resolver resolver{m_ctx};
        asio::ip::tcp::resolver::query query{host, port};

        std::error_code ec;
        auto endpoints = resolver.resolve(query, ec);
        if (ec) {
            log_error("resolve failed: {}", ec.message());
            cb(ec);
            return;
        }

        log_debug("addresses:");
        for (auto x : endpoints) {
            log_debug("ip address: {}", x.endpoint().address().to_string());
        }

        asio::async_connect(m_socket.lowest_layer(), std::move(endpoints),
                            [cb = std::move(cb), this](std::error_code ec,
                                                       const asio::ip::tcp::endpoint& e) mutable {
                                if (ec) {
                                    log_error("async_connect failed: {}", ec.message());
                                    cb(ec);
                                    return;
                                }

                                log_debug("connected: {}:{} (proto: {}), doing handhshake",
                                          e.address().to_string(), e.port(), e.protocol().family());
                                m_socket.async_handshake(
                                    asio::ssl::stream_base::client,
                                    [this, cb = std::move(cb)](std::error_code ec) mutable {
                                        if (ec) {
                                            log_error("async_handhshake failed: {}", ec.message());
                                            cb(ec);
                                            return;
                                        }
                                        log_debug("handshake done");

                                        m_connected = true;
                                        cb({});
                                    });
                            });
    }

    virtual void async_receive_line(async_callback<imap_response_line_t> cb) override {
        asio::async_read_until(
            m_socket, m_recv_buff, "\r\n",
            [this, cb = std::move(cb)](std::error_code ec, size_t bytes_transferred) mutable {
                if (ec) {
                    log_error("async_read_until failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                log_debug("bytes_transferred: {}, stream size: {}", bytes_transferred,
                          m_recv_buff.size());

                // TODO: ensure that stripping the line of \r\n is good idea. ABNF parses may not
                // like it and we may want to do it on upper level or have as parameter!

                const auto& buff_data = m_recv_buff.data();

                if (buff_data.size() < bytes_transferred) {
                    log_error("buff_data.size={} while bytes_transferred={}", buff_data.size(),
                              bytes_transferred);
                    cb(ec, {});
                    return;
                }

                const char* data_ptr = static_cast<const char*>(buff_data.data());

#ifndef NDEBUG
                if (m_opt_dump_stream_to_file) {
                    std::ofstream fs{"imap_socket_dump.bin", std::ios_base::out |
                                                                 std::ios_base::app |
                                                                 std::ios_base::binary};
                    fs.write(data_ptr, bytes_transferred);
                    if (!fs.good()) {
                        log_warning("dump failed");
                    }
                }
#endif

                std::string received_data{data_ptr, bytes_transferred};
                if (received_data.size() < 2 || received_data[received_data.size() - 2] != '\r' ||
                    received_data[received_data.size() - 1] != '\n') {
                    log_error("received not a line, no \\r\\n");
                    cb(make_error_code(std::errc::io_error), {});
                    return;
                }

                m_recv_buff.consume(bytes_transferred);
                cb({}, imap_response_line_t{std::move(received_data)});
            });
    }

    virtual void async_receive_raw_line(async_callback<std::string> cb) override {
        asio::async_read_until(
            m_socket, m_recv_buff, "\r\n",
            [this, cb = std::move(cb)](std::error_code ec, size_t bytes_transferred) mutable {
                if (ec) {
                    log_error("async_read_until failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                log_debug("bytes_transferred: {}, stream size: {}", bytes_transferred,
                          m_recv_buff.size());

                // TODO: ensure that stripping the line of \r\n is good idea. ABNF parses may not
                // like it and we may want to do it on upper level or have as parameter!

                const auto& buff_data = m_recv_buff.data();

                if (buff_data.size() < bytes_transferred) {
                    log_error("buff_data.size={} while bytes_transferred={}", buff_data.size(),
                              bytes_transferred);
                    cb(ec, {});
                    return;
                }

                const char* data_ptr = static_cast<const char*>(buff_data.data());

#ifndef NDEBUG
                if (m_opt_dump_stream_to_file) {
                    std::ofstream fs{"imap_socket_dump.bin", std::ios_base::out |
                                                                 std::ios_base::app |
                                                                 std::ios_base::binary};
                    fs.write(data_ptr, bytes_transferred);
                    if (!fs.good()) {
                        log_warning("dump failed");
                    }
                }
#endif

                std::string received_data{data_ptr, bytes_transferred};
                if (received_data.size() < 2 || received_data[received_data.size() - 2] != '\r' ||
                    received_data[received_data.size() - 1] != '\n') {
                    log_error("received not a line, no \\r\\n");
                    cb(make_error_code(std::errc::io_error), {});
                    return;
                }

                m_recv_buff.consume(bytes_transferred);
                cb({}, std::move(received_data));
            });
    }

    virtual void async_send_command(std::string command, async_callback<void> cb) override {
        if (!m_connected) {
            log_error("not connected");
            cb(make_error_code(std::errc::not_connected));
            return;
        }

        log_debug("sending command '{}'", utils::escape_ctrl(command));

        asio::async_write(m_socket, asio::buffer(command),
                          [cb = std::move(cb)](std::error_code ec, size_t bytes_written) mutable {
                              if (ec) {
                                  log_error("async_write failed: {}", ec.message());
                                  cb(make_error_code(std::errc::not_connected));
                                  return;
                              }

                              cb({});
                          });
    }

    virtual void set_option(imap_socket_opts::dump_stream_to_file) override {
        m_opt_dump_stream_to_file = true;
    }

   private:
    asio::io_context& m_ctx;
    asio::ssl::context m_ssl_ctx;
    asio::ssl::stream<asio::ip::tcp::socket> m_socket;
    bool m_connected = false;
    asio::streambuf m_recv_buff;
    bool m_opt_dump_stream_to_file = false;
};

}  // namespace

std::shared_ptr<imap_socket_t> make_imap_socket(asio::io_context& ctx) {
    return std::make_shared<imap_client_impl_t>(ctx);
}

void async_keep_receiving_lines_until(
    std::weak_ptr<imap_socket_t> socket_ptr,
    fu2::function<std::error_code(const imap_response_line_t& l)> p,
    async_callback<void> cb) {
    // TODO: what about the lifetime? what if socket gets deleted, this should rather be a member or
    // we should pass shared_ptr/weak_ptr to socket.

    auto socket = socket_ptr.lock();
    if (!socket) {
        cb(make_error_code(std::errc::owner_dead));
        return;
    }

    socket->async_receive_line([cb = std::move(cb), p = std::move(p), socket_ptr = socket](
                                   std::error_code ec, imap_response_line_t line) mutable {
        if (ec) {
            cb(ec);
            return;
        }

        auto predicate_ec = p(line);
        if (predicate_ec) {
            cb(predicate_ec);
        } else {
            async_keep_receiving_lines_until(socket_ptr, std::move(p), std::move(cb));
        }
    });
}

}  // namespace emailkit