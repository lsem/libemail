#include "imap_socket.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/ssl.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>

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

                                log_debug("connected: {}", e.address().to_string());
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

    virtual void async_receive_line(async_callback<std::string> cb) override {
        asio::async_read_until(
            m_socket, m_recv_buff, "\r\n",
            [this, cb = std::move(cb)](std::error_code ec, size_t bytes_transferred) mutable {
                log_debug("ec: {}, bytes_transferred: {}, stream size: {}", ec.message(),
                          bytes_transferred, m_recv_buff.size());

                if (ec) {
                    cb(ec, {});
                    return;
                }

                std::istream is{&m_recv_buff};
                std::string line;
                std::getline(is, line);
                cb({}, line);
            });
    }

    virtual void async_send_command(std::string command, async_callback<void> cb) override {
        if (!m_connected) {
            log_error("not connected");
            cb(make_error_code(std::errc::not_connected));
            return;
        }

        log_debug("sending command '{}'", command);

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

   private:
    asio::io_context& m_ctx;
    asio::ssl::context m_ssl_ctx;
    asio::ssl::stream<asio::ip::tcp::socket> m_socket;
    bool m_connected = false;
    asio::streambuf m_recv_buff;
};

}  // namespace

std::shared_ptr<imap_socket_t> make_imap_socket(asio::io_context& ctx) {
    return std::make_shared<imap_client_impl_t>(ctx);
}

}  // namespace emailkit