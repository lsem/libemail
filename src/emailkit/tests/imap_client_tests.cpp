#include <gtest/gtest.h>
#include <emailkit/imap_client.hpp>

#include <asio.hpp>
#include <asio/ssl.hpp>

using namespace emailkit;

const std::string gmail_imap_oauth2_success =
    "* OK Gimap ready for requests from 45.12.24.19 n18mb10782105ltg\r\n* CAPABILITY IMAP4rev1 "
    "UNSELECT IDLE NAMESPACE QUOTA ID XLIST CHILDREN X-GM-EXT-1 UIDPLUS COMPRESS=DEFLATE ENABLE "
    "MOVE CONDSTORE ESEARCH UTF8=ACCEPT LIST-EXTENDED LIST-STATUS LITERAL- SPECIAL-USE "
    "APPENDLIMIT=35651584\r\nA0 OK liubomyr.semkiv.test2@gmail.com authenticated (Success)\r\nˇ";

const std::string gmail_imap_no_token_failure =
    "* OK Gimap ready for requests from 45.12.24.19 n18mb10782105ltg\r\n* CAPABILITY IMAP4rev1 "
    "UNSELECT IDLE NAMESPACE QUOTA ID XLIST CHILDREN X-GM-EXT-1 UIDPLUS COMPRESS=DEFLATE ENABLE "
    "MOVE CONDSTORE ESEARCH UTF8=ACCEPT LIST-EXTENDED LIST-STATUS LITERAL- SPECIAL-USE "
    "APPENDLIMIT=35651584\r\nA0 OK liubomyr.semkiv.test2@gmail.com authenticated (Success)\r\n* OK "
    "Gimap ready for requests from 45.12.24.19 2adb3069b0e04-4fe633d2417mb6796542e87\r\n+ "
    "eyJzdGF0dXMiOiI0MDAiLCJzY2hlbWVzIjoiQmVhcmVyIiwic2NvcGUiOiJodHRwczovL21haWwuZ29vZ2xlLmNvbS8ifQ"
    "==\r\nA0 NO [AUTHENTICATIONFAILED] Invalid credentials (Failure)\r\nˇ";

class fake_imap_server {
   public:
    explicit fake_imap_server(asio::io_context& ctx, std::string host, std::string port)
        : m_ctx(ctx),
          m_host(host),
          m_port(port),
          m_ssl_ctx(asio::ssl::context::sslv23),
          m_acceptor(m_ctx) {
        m_ssl_ctx.set_options(asio::ssl::context::default_workarounds |
                              asio::ssl::context::no_sslv2 | asio::ssl::context::single_dh_use);
        m_ssl_ctx.use_certificate_chain_file("newcert.pem");
        m_ssl_ctx.use_private_key_file("privkey.pem", asio::ssl::context::pem);
    }

    std::error_code start() {
        std::error_code ec;

        asio::ip::tcp::resolver resolver(m_ctx);
        auto resolve_res = resolver.resolve(m_host, m_port, ec);
        if (ec) {
            log_error("resolve failed: {}", ec);
            return ec;
        }

        asio::ip::tcp::endpoint endpoint = *resolve_res.begin();
        m_acceptor.open(endpoint.protocol(), ec);
        if (ec) {
            log_error("open failed: {}", ec);
            return ec;
        }

        m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
        if (ec) {
            log_error("set_option failed: {}", ec);
            return ec;
        }

        m_acceptor.bind(endpoint, ec);
        if (ec) {
            log_error("bind failed: {}", ec);
            return ec;
        }

        m_acceptor.listen(128, ec);
        if (ec) {
            log_error("listen failed: {}", ec);
            return ec;
        }

        log_debug("started serving on: {}:{} (proto: {})", endpoint.address().to_string(),
                  endpoint.port(), endpoint.protocol().family());

        do_accept();

        return {};
    }

    void do_accept() {
        m_acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
            log_debug("accepted connection");
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
        log_debug("got connection on test server from: {}",
                  socket.remote_endpoint().address().to_string());
        m_ssl_socket = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket>>(std::move(socket),
                                                                                  m_ssl_ctx);
        m_ssl_socket->async_handshake(asio::ssl::stream_base::server, [this](std::error_code ec) {
            if (ec) {
                log_error("handshake failed: {}", ec);  // may hang I guess
                m_ssl_socket->lowest_layer().shutdown(asio::socket_base::shutdown_both, ec);
                if (ec) {
                    log_error("shutdown failed: {}", ec);
                    // ignore error.
                }
                m_ssl_socket->lowest_layer().close();
                return;
            }

            log_debug("handshake done");
            // std::string garbage = "garbage";
            // asio::async_write(*m_ssl_socket, asio::buffer(garbage), [](std::error_code ec, size_t
            // bytes_transfered) {
            //     log_info("async write result: {}", ec);
            // });
        });
        // socket.close();
    }

   private:
    asio::io_context& m_ctx;
    std::string m_host;
    std::string m_port;
    asio::ssl::context m_ssl_ctx;
    asio::ip::tcp::acceptor m_acceptor;
    std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> m_ssl_socket;
};

TEST(imap_client_test, gmail_imap_xoauth_success_test) {
    asio::io_context ctx;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    bool test_ran = false;

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);
        test_ran = true;

        client->async_authenticate({}, [](std::error_code ec, auto details) {

        });
    });

    ctx.run();

    EXPECT_TRUE(test_ran);
}