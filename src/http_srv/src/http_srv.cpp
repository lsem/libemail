#include "http_srv.hpp"
#include <asio.hpp>
#include <emailkit/log.hpp>
#include <set>
#include "connection.hpp"

namespace emailkit::http_srv {

class http_srv_impl_t : public http_srv_t, public std::enable_shared_from_this<http_srv_impl_t> {
   public:
    explicit http_srv_impl_t(asio::io_context& ctx, std::string host, std::string port)
        : m_ctx(ctx), m_acceptor(m_ctx), m_host(host), m_port(port) {}

    virtual bool start() override {
        std::error_code ec;

        asio::ip::tcp::resolver resolver(m_ctx);
        asio::ip::tcp::endpoint endpoint = *resolver.resolve(m_host, m_port, ec).begin();
        if (ec) {
            log_error("resolve failed: {}", ec);
            return false;
        }

        m_acceptor.open(endpoint.protocol(), ec);
        if (ec) {
            log_error("open fialed: {}", ec);
            return false;
        }

        m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
        if (ec) {
            log_error("set_option failed: {}", ec);
            return false;
        }

        m_acceptor.bind(endpoint, ec);
        if (ec) {
            log_error("bind fialed: {}", ec);
            return false;
        }

        m_acceptor.listen(128, ec);
        if (ec) {
            log_error("listen fialed: {}", ec);
            return false;
        }

        log_debug("started http server on {}:{}", m_host, m_port);

        do_accept();

        return true;
    }

    virtual void register_handler(std::string method, async_callback<std::string> cb) {}

    void do_accept() {
        m_acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!m_acceptor.is_open()) {
                // acceptor can be already closed before this callback had chance  to run.
                log_debug("accept is not open anymore, skipping");
                return;
            }

            if (ec) {
                log_error("async_accept failed: {}", ec);
            } else {
                log_debug("accepted client: {}", socket.local_endpoint().address().to_string());
                serve_client_async(std::move(socket));
            }

            do_accept();  // keep accepting
        });
    }

    void serve_client_async(asio::ip::tcp::socket socket) {
        auto conn = make_connection(
            m_ctx, std::move(socket),
            {.handle_request_cb =
                 [weak_this = weak_from_this()](const emailkit::http_srv::request&,
                                                async_callback<emailkit::http_srv::reply> cb) {
                     auto this_ptr = weak_this.lock();
                     if (!this_ptr) {
                         cb(make_error_code(std::errc::owner_dead), {});
                         return;
                     }

                     log_debug("connection requested to handle request");
                     cb({}, reply::stock_reply(reply::not_implemented));
                 },
             .connection_failed_cb =
                 [weak_this = weak_from_this()] {
                     auto this_ptr = weak_this.lock();
                     if (!this_ptr) {
                         return;
                     }
                     log_warning("connection requested to handle connection failure");
                     // TODO: how to find itself in m_active_conns and remove it?
                 }});
        m_active_conns.emplace(conn);
        conn->start();
    }

   private:
    asio::io_context& m_ctx;
    asio::ip::tcp::acceptor m_acceptor;
    std::string m_host;
    std::string m_port;
    std::set<std::shared_ptr<connection_t>> m_active_conns;
};

std::shared_ptr<http_srv_t> make_http_srv(asio::io_context& ctx,
                                          std::string host,
                                          std::string port) {
    return std::make_shared<http_srv_impl_t>(ctx, host, port);
}

}  // namespace emailkit::http_srv