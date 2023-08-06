#include <gtest/gtest.h>
#include <emailkit/imap_client.hpp>
#include <emailkit/utils.hpp>

#include <asio.hpp>
#include <asio/read_until.hpp>
#include <asio/ssl.hpp>

#include <list>

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

    void reply_once(async_callback<std::tuple<std::string, async_callback<std::string>>> cb) {
        m_pending_reply = std::move(cb);
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

    void serve_mocked_version() {
        log_debug("waiting line from client..");
        asio::async_read_until(
            *m_ssl_socket, m_recv_buff, "\r\n",
            [this](std::error_code ec, size_t bytes_transfered) {
                if (ec) {
                    log_error("async_read_until failed: {}", ec);
                    close_conn();
                    return;
                }

                const auto& buff_data = m_recv_buff.data();
                const char* data_ptr = static_cast<const char*>(buff_data.data());
                std::string line(data_ptr, data_ptr + bytes_transfered);
                m_recv_buff.consume(bytes_transfered);

                if (m_pending_reply) {
                    auto pending_reply = std::exchange(m_pending_reply, nullptr);

                    pending_reply(
                        std::error_code{},
                        std::tuple{
                            line,
                            [this](std::error_code ec, std::string data) {
                                asio::async_write(
                                    *m_ssl_socket, asio::buffer(data),
                                    [this, data](std::error_code ec, size_t bytes_transfered) {
                                        if (ec) {
                                            log_error("writing reply failed: {}", ec);
                                            close_conn();
                                            return;
                                        }

                                        log_debug("line '{}' has been sent  to the client",
                                                  utils::escape_ctrl(data));

                                        serve_mocked_version();
                                    });
                            }

                        });
                }
            });
    }

    void serve_client_per_behavior() {
        // TODO: this is success behavior
        log_debug("waiting line from client..");
        asio::async_read_until(
            *m_ssl_socket, m_recv_buff, "\r\n",
            [this](std::error_code ec, size_t bytes_transfered) {
                const auto& buff_data = m_recv_buff.data();
                const char* data_ptr = static_cast<const char*>(buff_data.data());
                std::string line(data_ptr, data_ptr + bytes_transfered);
                m_recv_buff.consume(bytes_transfered);
                log_debug("server got a line: '{}' (of size {}))",
                          utils::replace_control_chars(line), line.size());

                // example of command
                // A0 AUTHENTICATE XOAUTH2 dXNlcj0BYXV0aD1CZWFyZXIgAQ\r\n
                if (line.find("AUTHENTICATE") != std::string::npos) {
                    const auto tokens = utils::split(line, ' ');
                    log_debug("tokens: ({})", tokens);
                    if (tokens.size() < 3) {
                        log_error("unexpected line from client, closing connection");
                        close_conn();
                        return;
                    }

                    std::string response;
                    response += tokens[0];
                    response += " OK liubomyr.semkiv.test2@gmail.com authenticated (Success)\r\n";
                    asio::async_write(*m_ssl_socket, asio::buffer(response),
                                      [this](std::error_code ec, size_t bytes_transfered) {
                                          if (ec) {
                                              log_error("failed sending reply: {}", ec);
                                              close_conn();
                                              return;
                                          }

                                          m_authenticated = true;

                                          log_debug("reply sent, reading next command..");
                                          serve_client_per_behavior();
                                      });
                } else {
                    log_warning("unknown command: {}", utils::replace_control_chars(line));
                    close_conn();
                }
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

            serve_mocked_version();
        });
    }

    void close_conn() {
        if (!m_ssl_socket->lowest_layer().is_open()) {
            log_warning("already closed");
            return;
        }
        std::error_code ec;
        m_ssl_socket->lowest_layer().shutdown(asio::socket_base::shutdown_both, ec);
        if (ec) {
            log_error("shutdown failed: {}", ec);
            // ignore error.
        }
        m_ssl_socket->lowest_layer().close(ec);
        if (ec) {
            log_error("close failed: {}", ec);
            // ignore error.
        }
    }

   private:
    asio::io_context& m_ctx;
    std::string m_host;
    std::string m_port;
    asio::ssl::context m_ssl_ctx;
    asio::ip::tcp::acceptor m_acceptor;
    std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> m_ssl_socket;
    asio::streambuf m_recv_buff;
    bool m_authenticated = false;

    async_callback<std::tuple<std::string, async_callback<std::string>>> m_pending_reply;
};

struct imap_command_t {
    std::vector<std::string> tokens;
};

std::optional<imap_command_t> parse_imap_command(std::string s) {
    auto tokens = utils::split(s, ' ');
    if (tokens.size() < 2) {
        log_error("invalid imap command: {}", s);
        return {};
    }
    return imap_command_t{.tokens = tokens};
}

TEST(imap_client_test, gmail_imap_xoauth_success_test) {
    asio::io_context ctx;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    bool test_ran = false;

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        // Instruct server reply once with the following:
        srv.reply_once([&](std::error_code ec,
                           std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            // that is we who send this command and we expect to have it valid.
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            log_debug("client sent: {}", cmd.tokens);

            ASSERT_EQ(cmd.tokens.size(), 4);

            EXPECT_EQ(cmd.tokens[1], "AUTHENTICATE");
            EXPECT_EQ(cmd.tokens[2], "XOAUTH2");

            auto decoded_tok3 = utils::base64_naive_decode(cmd.tokens[3]);
            log_debug("decoded_token: {}", utils::replace_control_chars(decoded_tok3));

            EXPECT_EQ(decoded_tok3[decoded_tok3.size() - 1], 0x01);
            EXPECT_EQ(decoded_tok3[decoded_tok3.size() - 2], 0x01);

            EXPECT_EQ(utils::replace_control_chars(decoded_tok3),
                      "user=alan.kay@example.com^Aauth=Bearer [alan_kay_oauth_token]^A^A");

            cb({}, fmt::format("{} OK\r\n", cmd.tokens[0]));
        });

        client->async_authenticate(
            {.user_email = "alan.kay@example.com", .oauth_token = "[alan_kay_oauth_token]"},
            [&](std::error_code ec, auto details) {
                ASSERT_FALSE(ec);
                ctx.stop();
                test_ran = true;
            });
    });

    ctx.run_for(std::chrono::seconds(1));

    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, gmail_imap_xoauth_failure_400_test) {
    // Test recorded from experiments with gmail imap server. In this particular example gmail imap
    // complains about lack of scope.
    asio::io_context ctx;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    bool test_ran = false;

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

}