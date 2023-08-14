#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <emailkit/imap_client.hpp>
#include <emailkit/utils.hpp>

#include <asio.hpp>
#include <asio/read_until.hpp>
#include <asio/ssl.hpp>

#include <list>

using namespace emailkit;
using namespace emailkit::imap_client;
using namespace testing;

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

    using reply_once_cb_t = async_callback<std::tuple<std::string, async_callback<std::string>>>;

    void reply_once(reply_once_cb_t cb) { m_pending_reply.emplace_back(std::move(cb)); }

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

                if (!m_pending_reply.empty()) {
                    auto pending_reply = std::move(m_pending_reply.front());
                    m_pending_reply.pop_front();

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
                } else {
                    // The test must be wrong or client has some bug.
                    // TODO: consider having strict semantics to catch bugs when client sends
                    // unexpectes stuff.
                    log_warning("unexpected call from client, no reply, closing connection..");
                    close_conn();
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

    std::list<reply_once_cb_t> m_pending_reply;
};

struct imap_command_t {
    std::vector<std::string> tokens;
};

std::optional<imap_command_t> parse_imap_command(std::string s) {
    if (s.size() < 2) {
        log_error("imap command cannot be less than 2 chars (crlf)");
        return {};
    }
    if (s[s.size() - 2] != '\r' || s[s.size() - 1] != '\n') {
        log_error("no crlf in command");
        return {};
    }
    s.resize(s.size() - 2);  // trim crlf

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

    // Instruct server reply once with the following:
    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
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

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

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

    // When gmail does not like our auth data it responds with explanation which is reported
    // as challenge "+ <BASE64>\r\n" which clients needs to read and send \r\n in reply after which
    // server will replies with "<ID> NO".
    std::string auth_command_id;

    bool test_ran = false;
    bool error_challange_accepted = false;

    srv.reply_once([&](std::error_code ec,
                       std::tuple<std::string, async_callback<std::string>> line_and_cb) {
        auto& [line, cb] = line_and_cb;

        auto maybe_cmd = parse_imap_command(line);
        // that is we who send this command and we expect to have it valid.
        ASSERT_TRUE(maybe_cmd);
        auto& cmd = *maybe_cmd;

        ASSERT_GT(cmd.tokens.size(), 3);
        EXPECT_EQ(cmd.tokens[1], "AUTHENTICATE");
        EXPECT_EQ(cmd.tokens[2], "XOAUTH2");

        // save command id so we can reply with it after challange.
        auth_command_id = cmd.tokens[0];

        const std::string json_reply =
            R"json({"status" : "400", "schemes" : "Bearer", "scope" : "https://mail.google.com/" })json";

        cb({}, fmt::format("+ {}\r\n", utils::base64_naive_encode(json_reply)));
    });

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;
            EXPECT_EQ(line, "\r\n");
            error_challange_accepted = true;
            cb({}, fmt::format("{} NO [AUTHENTICATIONFAILED] Invalid credentials (Failure)\r\n",
                               auth_command_id));
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_authenticate(
            {.user_email = "niklaus.wirth@example.com", .oauth_token = "[virth_token]"},
            [&](std::error_code ec, auto details) {
                ASSERT_TRUE(ec);  // TODO: concrete error?
                ASSERT_NE(ec,
                          make_error_code(lsem::async_kit::errors::async_callback_err::not_called));
                EXPECT_EQ(
                    details.summary,
                    R"json({"status" : "400", "schemes" : "Bearer", "scope" : "https://mail.google.com/" })json");
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
    EXPECT_TRUE(error_challange_accepted);
}

TEST(imap_client_test, imap_xoauth_failure_no_challange_test) {
    // haven't check but it is reasonable that server is not oblicated to respond with challange
    // and can just return NO (TODO: separate test for "BAD") and separate test for timeout?
    // https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth

    asio::io_context ctx;

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            // that is we who send this command and we expect to have it valid.
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            ASSERT_GT(cmd.tokens.size(), 3);
            EXPECT_EQ(cmd.tokens[1], "AUTHENTICATE");
            EXPECT_EQ(cmd.tokens[2], "XOAUTH2");

            cb({}, fmt::format("{} NO AUTHENTICATE failed.\r\n", cmd.tokens[0]));
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_authenticate(
            {.user_email = "niklaus.wirth@example.com", .oauth_token = "[virth_token]"},
            [&](std::error_code ec, auto details) {
                ASSERT_TRUE(ec);
                ASSERT_NE(ec,
                          make_error_code(lsem::async_kit::errors::async_callback_err::not_called));
                EXPECT_EQ(details.summary, "");
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, list_command_basic_test) {
    asio::io_context ctx;

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once([&](std::error_code ec,
                       std::tuple<std::string, async_callback<std::string>> line_and_cb) {
        auto& [line, cb] = line_and_cb;

        auto maybe_cmd = parse_imap_command(line);
        ASSERT_TRUE(maybe_cmd);
        auto& cmd = *maybe_cmd;

        // TODO: check that cmd.tokens[0] is valid tag, check that two commands in a row have
        // uniqu tags.

        ASSERT_GT(cmd.tokens.size(), 3);
        EXPECT_EQ(cmd.tokens[1], "list");
        EXPECT_EQ(cmd.tokens[2], R"("")");   // "" this thing called reference name
        EXPECT_EQ(cmd.tokens[3], R"("*")");  // "" mailbox name with possible wildcards

        // clang-format off
    std::string response = 
        R"(* LIST (\HasNoChildren) "/" "INBOX")""\r\n"
        R"(* LIST (\A1 \A2) "/" "INBOX")""\r\n"
        R"(* LIST (\AnyFlag \AnierFlag) "/" "[Gmail]")""\r\n"
        R"(* LIST (\HasChildren \Noselect) "/" "[Gmail]")""\r\n"
        R"(* LIST (\Flagged \HasNoChildren) "/" "[Gmail]/&BAYENw- &BDcEVgRABD4ERwQ6BD4ETg-")""\r\n"
        R"(* LIST (\HasNoChildren \Important) "/" "[Gmail]/&BBIEMAQ2BDsEOAQyBD4-")""\r\n"
        R"(* LIST (\HasNoChildren \Trash) "/" "[Gmail]/&BBoEPgRIBDgEOg-")""\r\n"
        R"(* LIST (\HasNoChildren \Sent) "/" "[Gmail]/&BB0EMAQ0BFYEQQQ7BDAEPQRW-")""\r\n"
        R"(* LIST (\HasNoChildren \Junk) "/" "[Gmail]/&BCEEPwQwBDw-")""\r\n"
        R"(* LIST (\All \HasNoChildren) "/" "[Gmail]/&BCMEQQRP- &BD8EPgRIBEIEMA-")""\r\n"
        R"(* LIST (\Drafts \HasNoChildren) "/" "[Gmail]/&BCcENQRABD0ENQRCBDoEOA-")""\r\n";
        // clang-format on

        response += fmt::format("{} OK Success\r\n", cmd.tokens[0]) + "\r\n";

        cb({}, response);
    });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            emailkit::imap_client::imap_commands::list_t{.reference_name = "", .mailbox_name = "*"},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_FALSE(ec);
                EXPECT_EQ(r.inbox_list.size(), 11);

                EXPECT_THAT(r.inbox_list[0].flags, ElementsAre("\\HasNoChildren"));
                EXPECT_THAT(r.inbox_list[0].inbox_path, ElementsAre("INBOX"));

                EXPECT_THAT(r.inbox_list[1].flags, ElementsAre("\\A1", "\\A2"));
                EXPECT_THAT(r.inbox_list[1].inbox_path, ElementsAre("INBOX"));

                EXPECT_THAT(r.inbox_list[2].flags, ElementsAre("\\AnyFlag", "\\AnierFlag"));
                EXPECT_THAT(r.inbox_list[2].inbox_path, ElementsAre("[Gmail]"));

                EXPECT_THAT(r.inbox_list[3].inbox_path, ElementsAre("[Gmail]"));
                EXPECT_THAT(r.inbox_list[3].flags, ElementsAre("\\HasChildren", "\\Noselect"));

                EXPECT_THAT(r.inbox_list[4].flags, ElementsAre("\\Flagged", "\\HasNoChildren"));
                EXPECT_THAT(r.inbox_list[4].inbox_path, ElementsAre("[Gmail]", "Із зірочкою"));

                EXPECT_THAT(r.inbox_list[5].flags, ElementsAre("\\HasNoChildren", "\\Important"));
                EXPECT_THAT(r.inbox_list[5].inbox_path, ElementsAre("[Gmail]", "Важливо"));

                EXPECT_THAT(r.inbox_list[6].flags, ElementsAre("\\HasNoChildren", "\\Trash"));
                EXPECT_THAT(r.inbox_list[6].inbox_path, ElementsAre("[Gmail]", "Кошик"));

                EXPECT_THAT(r.inbox_list[7].flags, ElementsAre("\\HasNoChildren", "\\Sent"));
                EXPECT_THAT(r.inbox_list[7].inbox_path, ElementsAre("[Gmail]", "Надіслані"));

                EXPECT_THAT(r.inbox_list[8].flags, ElementsAre("\\HasNoChildren", "\\Junk"));
                EXPECT_THAT(r.inbox_list[8].inbox_path, ElementsAre("[Gmail]", "Спам"));

                EXPECT_THAT(r.inbox_list[9].flags, ElementsAre("\\All", "\\HasNoChildren"));
                EXPECT_THAT(r.inbox_list[9].inbox_path, ElementsAre("[Gmail]", "Уся пошта"));

                EXPECT_THAT(r.inbox_list[10].flags, ElementsAre("\\Drafts", "\\HasNoChildren"));
                EXPECT_THAT(r.inbox_list[10].inbox_path, ElementsAre("[Gmail]", "Чернетки"));

                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, DISABLED_list_command_windows_style_delimiter) {
    asio::io_context ctx;

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once([&](std::error_code ec,
                       std::tuple<std::string, async_callback<std::string>> line_and_cb) {
        auto& [line, cb] = line_and_cb;

        auto maybe_cmd = parse_imap_command(line);
        ASSERT_TRUE(maybe_cmd);
        auto& cmd = *maybe_cmd;

        // TODO: check that cmd.tokens[0] is valid tag, check that two commands in a row have
        // uniqu tags.

        ASSERT_GT(cmd.tokens.size(), 3);
        EXPECT_EQ(cmd.tokens[1], "list");
        EXPECT_EQ(cmd.tokens[2], R"("")");   // "" this thing called reference name
        EXPECT_EQ(cmd.tokens[3], R"("*")");  // "" mailbox name with possible wildcards

        // clang-format off
    std::string response = 
        R"(* LIST (\Flagged \HasNoChildren) "\" "[Gmail]\&BAYENw- &BDcEVgRABD4ERwQ6BD4ETg-")""\r\n";
        // clang-format on

        response += fmt::format("{} OK Success\r\n", cmd.tokens[0]) + "\r\n";

        cb({}, response);
    });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            emailkit::imap_client::imap_commands::list_t{},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_FALSE(ec);
                EXPECT_EQ(r.inbox_list.size(), 1);

                EXPECT_THAT(r.inbox_list[0].flags, ElementsAre("\\Flagged", "\\HasNoChildren"));
                EXPECT_THAT(r.inbox_list[0].inbox_path, ElementsAre("[Gmail]", "Із зірочкою"));
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, list_command_invalid_response) {
    asio::io_context ctx;

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            // TODO: check that cmd.tokens[0] is valid tag, check that two commands in a row have
            // uniqu tags.

            ASSERT_GT(cmd.tokens.size(), 3);
            EXPECT_EQ(cmd.tokens[1], "list");
            EXPECT_EQ(cmd.tokens[2], R"("reference/")");  // "" this thing called reference name
            EXPECT_EQ(cmd.tokens[3], R"("*")");           // "" mailbox name with possible wildcards

            // clang-format off
    std::string response = 
    // No delimiter in response, bad grammar.
        R"(* LIST (\Flagged \HasNoChildren) [Gmail]/&BAYENw- &BDcEVgRABD4ERwQ6BD4ETg-")""\r\n";
            // clang-format on

            response += fmt::format("{} OK Success\r\n", cmd.tokens[0]) + "\r\n";

            cb({}, response);
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            emailkit::imap_client::imap_commands::list_t{.reference_name = "reference/",
                                                         .mailbox_name = "*"},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_FALSE(ec);  // ERROR IGNORED!
                EXPECT_EQ(r.inbox_list.size(), 0);
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, list_command_NO_response) {
    // according to RFC, list commands can return:
    //     NO - list failure: can't list that reference or name

    asio::io_context ctx;

    // I could not find how realword NO looks like so this test is from my understading of this.
    // Please update test with realistic example if one ever finds it.
    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            // TODO: check that cmd.tokens[0] is valid tag, check that two commands in a row have
            // uniqu tags.

            ASSERT_GT(cmd.tokens.size(), 3);
            EXPECT_EQ(cmd.tokens[1], "list");
            // No expectations here.

            cb({}, fmt::format("{} NO Failed.\r\n", cmd.tokens[0]));
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            // '"' as reference name makes response bad (tested on gmail).
            imap_client::imap_commands::list_t{.reference_name = "SOME WRONG REF",
                                               .mailbox_name = "SOME WRONG MAILBOX"},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_TRUE(ec);
                ASSERT_EQ(ec, emailkit::imap_client::types::imap_errors::imap_no);
                EXPECT_EQ(r.inbox_list.size(), 0);
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, list_command_rfc_example_2) {
    asio::io_context ctx;

    // Example 2 from RFC.

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            ASSERT_GT(cmd.tokens.size(), 3);

            const std::string response = fmt::format(
                "* LIST (\\Noselect) \".\" #news.\r\n"
                "{} OK LIST completed\r\n",
                cmd.tokens[0]);
            cb({}, response);
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            emailkit::imap_client::imap_commands::list_t{.reference_name = "#news.comp.mail.misc",
                                                         .mailbox_name = ""},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_FALSE(ec);
                ASSERT_EQ(r.inbox_list.size(), 1);
                EXPECT_THAT(r.inbox_list[0].flags, ElementsAre("\\Noselect"));
                EXPECT_THAT(r.inbox_list[0].inbox_path, ElementsAre("#news"));
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, list_command_rfc_example_1) {
    asio::io_context ctx;

    // Example 1 from RFC.

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            ASSERT_GT(cmd.tokens.size(), 3);

            const std::string response = fmt::format(
                "* LIST (\\Noselect) \"/\" \"\"\r\n"
                "{} OK LIST completed\r\n",
                cmd.tokens[0]);
            cb({}, response);
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            emailkit::imap_client::imap_commands::list_t{},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_FALSE(ec);
                ASSERT_EQ(r.inbox_list.size(), 1);
                EXPECT_THAT(r.inbox_list[0].flags, ElementsAre("\\Noselect"));
                EXPECT_THAT(r.inbox_list[0].inbox_path, ElementsAre());
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, list_command_rfc_example_3) {
    asio::io_context ctx;

    // Example 3 from RFC.

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            ASSERT_GT(cmd.tokens.size(), 3);

            const std::string response = fmt::format(
                "* LIST (\\Noselect) \"/\" /\r\n"
                "{} OK LIST completed\r\n",
                cmd.tokens[0]);
            cb({}, response);
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            emailkit::imap_client::imap_commands::list_t{.reference_name = "/usr/staff/jones",
                                                         .mailbox_name = ""},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_FALSE(ec);
                ASSERT_EQ(r.inbox_list.size(), 1);
                EXPECT_THAT(r.inbox_list[0].flags, ElementsAre("\\Noselect"));
                EXPECT_THAT(r.inbox_list[0].inbox_path, ElementsAre());
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

TEST(imap_client_test, list_command_rfc_example_4) {
    asio::io_context ctx;

    // Example 4 from RFC.

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            ASSERT_GT(cmd.tokens.size(), 3);

            //ASSERT_GT(cmd.tokens.size(), 3);
            // TODO: FIXME: put expectations that there is no ""

            const std::string response = fmt::format(
                "* LIST (\\Noselect) \"/\" ~/Mail/foo\r\n"
                "* LIST () \"/\" ~/Mail/meetings\r\n"
                "{} OK LIST completed\r\n",
                cmd.tokens[0]);
            cb({}, response);
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            emailkit::imap_client::imap_commands::list_t{.reference_name = "~/Mail/",
                                                         .mailbox_name = "%"},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_FALSE(ec);
                ASSERT_EQ(r.inbox_list.size(), 2);
                EXPECT_THAT(r.inbox_list[0].flags, ElementsAre("\\Noselect"));
                EXPECT_THAT(r.inbox_list[0].inbox_path, ElementsAre("~", "Mail", "foo"));
                EXPECT_THAT(r.inbox_list[1].flags, ElementsAre());
                EXPECT_THAT(r.inbox_list[1].inbox_path, ElementsAre("~", "Mail", "meetings"));
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

// TODO: unexpected tag from server.

TEST(imap_client_test, list_command_BAD_response) {
    asio::io_context ctx;

    // Example of Gmail response:
    // 'A2 BAD Could not parse command\r\n'

    bool test_ran = false;

    fake_imap_server srv{ctx, "localhost", "9934"};
    ASSERT_FALSE(srv.start());

    srv.reply_once(
        [&](std::error_code ec, std::tuple<std::string, async_callback<std::string>> line_and_cb) {
            auto& [line, cb] = line_and_cb;

            auto maybe_cmd = parse_imap_command(line);
            ASSERT_TRUE(maybe_cmd);
            auto& cmd = *maybe_cmd;

            // TODO: check that cmd.tokens[0] is valid tag, check that two commands in a row have
            // uniqu tags.

            ASSERT_GT(cmd.tokens.size(), 3);
            EXPECT_EQ(cmd.tokens[1], "list");
            EXPECT_EQ(cmd.tokens[2], "\"\"\"");  // "" this thing called reference name
            EXPECT_EQ(cmd.tokens[3], "\"*\"");   // "" mailbox name with possible wildcards

            cb({}, fmt::format("{} BAD Could not parse command\r\n", cmd.tokens[0]));
        });

    auto client = make_imap_client(ctx);
    client->async_connect("localhost", "9934", [&](std::error_code ec) {
        ASSERT_FALSE(ec);

        client->async_execute_command(
            // '"' as reference name makes response bad (tested on gmail).
            imap_client::imap_commands::list_t{.reference_name = "\"", .mailbox_name = "*"},
            [&](std::error_code ec, emailkit::imap_client::types::list_response_t r) {
                ASSERT_TRUE(ec);
                ASSERT_EQ(ec, emailkit::imap_client::types::imap_errors::imap_bad);
                EXPECT_EQ(r.inbox_list.size(), 0);
                test_ran = true;
                ctx.stop();
            });
    });

    ctx.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(test_ran);
}

// TODO: test for when server responds with * during authentication.