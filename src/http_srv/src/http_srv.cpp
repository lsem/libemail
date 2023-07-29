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

    virtual void register_handler(std::string method, std::string pattern, handler_func_t func) {
        m_registered_handelrs.emplace_back(
            handler_tuple{.method = method, .pattern = pattern, .func = std::move(func)});
    }

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

    std::string socket_id(const asio::ip::tcp::socket& s) {
        return fmt::format("{}:{}", s.local_endpoint().address().to_string(),
                           s.local_endpoint().port());
    }

    void serve_client_async(asio::ip::tcp::socket socket) {
        const auto sock_id = socket_id(socket);
        auto conn = make_connection(
            m_ctx, std::move(socket),
            {.handle_request_cb =
                 [sock_id, weak_this = weak_from_this()](
                     const emailkit::http_srv::request& req,
                     async_callback<emailkit::http_srv::reply> cb) {
                     auto this_ptr = weak_this.lock();
                     if (!this_ptr) {
                         // TODO: shouldn't we respond with some HTTP 500?
                         cb(make_error_code(std::errc::owner_dead), {});
                         return;
                     }
                     auto& this_ = *this_ptr;

                     if (auto handler_it = this_.find_matching_handler(req);
                         handler_it != this_.m_registered_handelrs.end()) {
                         log_debug("executing handler");
                         handler_it->func(
                             req, [cb = std::move(cb)](std::error_code ec,
                                                       emailkit::http_srv::reply reply) mutable {
                                 // TODO: map not-called errror into HTTP 500.
                                 cb(ec, std::move(reply));
                             });
                     } else {
                         log_debug("no handler for connection");
                         cb({}, reply::stock_reply(reply::not_implemented));
                     }
                 },
             .connection_failed_cb =
                 [weak_this = weak_from_this(), sock_id] {
                     auto this_ptr = weak_this.lock();
                     if (!this_ptr) {
                         return;
                     }
                     log_warning("connection requested to handle connection failure");
                     // TODO: how to find itself in m_active_conns and remove it?
                 }});
        m_active_conns.emplace(sock_id, conn);
        conn->start();
    }

    struct handler_tuple {
        std::string method;
        std::string pattern;
        handler_func_t func;
    };
    using registered_handlers_t = std::vector<handler_tuple>;

    const registered_handlers_t::iterator find_matching_handler(
        const emailkit::http_srv::request& r) {
        log_debug("have {} handlers", m_registered_handelrs.size());
        for (auto it = m_registered_handelrs.begin(); it != m_registered_handelrs.end(); ++it) {
            auto& h = *it;

            const bool method_match =
                h.method.size() == r.method.size() &&
                std::equal(h.method.begin(), h.method.end(), r.method.begin(), r.method.end(),
                           [](char c1, char c2) { return ::toupper(c1) == ::toupper(c2); });
            if (!method_match) {
                log_debug("method not matched: {} != {}", h.method, r.method);
                continue;
            }

            log_debug("trying to match pattern '{}' to uri '{}'", h.pattern, r.uri);
            if (r.uri.find(h.pattern) != std::string::npos) {
                log_debug("found prefix match '' and ''", h.pattern, r.uri);
                return it;
            }
        }

        return m_registered_handelrs.end();
    }

   private:
    asio::io_context& m_ctx;
    asio::ip::tcp::acceptor m_acceptor;
    std::string m_host;
    std::string m_port;

    using active_conns_t = std::set<std::pair<std::string, std::shared_ptr<connection_t>>>;
    active_conns_t m_active_conns;

    registered_handlers_t m_registered_handelrs;
};

std::shared_ptr<http_srv_t> make_http_srv(asio::io_context& ctx,
                                          std::string host,
                                          std::string port) {
    return std::make_shared<http_srv_impl_t>(ctx, host, port);
}

}  // namespace emailkit::http_srv