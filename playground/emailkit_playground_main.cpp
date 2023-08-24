#include <emailkit/global.hpp>
#include <fmt/ranges.h>
#include <emailkit/emailkit.hpp>
#include <emailkit/google_auth.hpp>
#include <emailkit/http_srv.hpp>
#include <emailkit/imap_client.hpp>
#include <emailkit/imap_parser.hpp>
#include <emailkit/imap_parser_utils.hpp>
#include <emailkit/imap_socket.hpp>
#include <emailkit/log.hpp>
#include <emailkit/uri_codec.hpp>
#include <emailkit/utils.hpp>
#include <folly/folly_uri.hpp>
#include <iostream>

#include <gmime/gmime.h>

using namespace emailkit;

// https://gist.github.com/karanth/8420579
void gmail_auth_test() {
    // {
    //    "installed":{
    //       "client_id":"303173870696-tpi64a42emnt758cjn3tqp2ukncggof9.apps.googleusercontent.com",
    //       "project_id":"glowing-bolt-393519",
    //       "auth_uri":"https://accounts.google.com/o/oauth2/auth",
    //       "token_uri":"https://oauth2.googleapis.com/token",
    //       "auth_provider_x509_cert_url":"https://www.googleapis.com/oauth2/v1/certs",
    //       "client_secret":"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",
    //       "redirect_uris":[
    //          "http://localhost"
    //       ]
    //    }
    // }

    asio::io_context ctx;
    const auto app_creds = emailkit::google_auth_app_creds_t{
        .client_id = "303173870696-bsun94hmoseeumiat4iaa6dr752ta805.apps.googleusercontent.com",
        .client_secret = "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"};

    // Note the app should also have scopes selected and proper test users to make it working.
    // const std::vector<std::string> scopes = {"https://mail.google.com/",
    //                                          "https://www.googleapis.com/auth/userinfo.email",
    //                                          "https://www.googleapis.com/auth/userinfo.profile"};
    const std::vector<std::string> scopes = {"https://mail.google.com"};

    auto imap_client = emailkit::imap_client::make_imap_client(ctx);

    // We are requesting authentication for our app (represented by app_creds) for scoped needed for
    // our application.
    auto google_auth = emailkit::make_google_auth(ctx, "127.0.0.1", "8089");
    google_auth->async_handle_auth(app_creds, scopes, [&](std::error_code ec, auth_data_t auth_data) {
        if (ec) {
            log_error("async_handle_auth failed: {}", ec);
            return;
        }

        log_info("received creds: {}", auth_data);

        log_debug("connecting to GMail IMAP endpoint..");

        imap_client->async_connect("imap.gmail.com", "993", [&, auth_data](std::error_code ec) {
            if (ec) {
                log_error("failed connecting Gmail IMAP: {}", ec);
                return;
            }

            log_info("connected GMail IMAP, authenticating via SASL");

            // TODO: why user is not provided from google auth?
            const std::string user = "liubomyr.semkiv.test2@gmail.com";

            // imap_client->async_obtain_capabilities(
            //     [&, user, auth_data](std::error_code ec, std::vector<std::string> caps) {
            //         if (ec) {
            //             log_error("failed otaining capabilities: {}", ec);
            //             return;
            //         }

            imap_client->async_authenticate(
                {.user_email = user, .oauth_token = auth_data.access_token},
                [&](std::error_code ec, emailkit::imap_client::auth_error_details_t err_details) {
                    if (ec) {
                        log_error("async_authenticate failed: {}{}", ec,
                                  err_details.summary.empty()
                                      ? ""
                                      : fmt::format(", summary: {})", err_details.summary));
                        return;
                    }

                    log_info("authenticated to gimap");

                    imap_client->async_execute_command(
                        imap_client::imap_commands::namespace_t{}, [&](std::error_code ec) {
                            if (ec) {
                                log_error("failed executing ns command: {}", ec);
                                return;
                            }

                            log_info("executed namespace command");

                            imap_client->async_execute_command(
                                imap_client::imap_commands::list_t{.reference_name = "",
                                                                   .mailbox_name = "*"},
                                [&](std::error_code ec,
                                    imap_client::types::list_response_t response) {
                                    if (ec) {
                                        log_error("failed exeucting list command: {}", ec);
                                        return;
                                    }

                                    log_info("executed list command:\n{}",
                                             fmt::join(response.inbox_list, "\n"));
                                    // for (auto& entry : response.inbox_list) {
                                    //     log_info("    {}", entry);
                                    // }

                                    imap_client->async_execute_command(
                                        imap_client::imap_commands::select_t{.mailbox_name =
                                                                                 "\"INBOX\""},

                                        // imap_client::imap_commands::select_t{.mailbox_name =
                                        //                                          "\"[Gmail]/&BCcENQRABD0ENQRCBDoEOA-\""},
                                        [&](std::error_code ec) {
                                            if (ec) {
                                                log_error("gmail command failed: {}", ec);
                                            }
                                        });
                                });
                        });
                });
        });
        // });
    });

    ctx.run();
}

void http_srv_test() {
    asio::io_context ctx;
    auto srv = http_srv::make_http_srv(ctx, "localhost", "8087");
    srv->register_handler("get", "/",
                          [](const http_srv::request& req, async_callback<http_srv::reply> cb) {
                              cb({}, http_srv::reply::stock_reply(http_srv::reply::forbidden));
                          });
    srv->register_handler("get", "/auth",
                          [](const http_srv::request& req, async_callback<http_srv::reply> cb) {
                              cb({}, http_srv::reply::stock_reply(http_srv::reply::not_found));
                          });
    srv->register_handler("get", "/auth_exchange",
                          [](const http_srv::request& req, async_callback<http_srv::reply> cb) {
                              cb({}, http_srv::reply::stock_reply(http_srv::reply::unauthorized));
                          });
    srv->start();
    ctx.run();
}

void imap_socket_test() {
    asio::io_context ctx;
    auto imap_socket = emailkit::make_imap_socket(ctx);
    const std::string gmail_imap_uri = "imap.gmail.com";

    async_callback<imap_response_line_t> async_receive_line_cb;

    async_receive_line_cb = [&async_receive_line_cb, &imap_socket](std::error_code ec,
                                                                   imap_response_line_t line) {
        if (ec) {
            if (ec == asio::error::eof) {
                log_warning("connection closed by the server (eof)");
                // TODO: what should we do?
            } else {
                // receive error
                log_error("imap client run failed: {}", ec.message());
                // TODO: reconnect or restart? I guess in realistic problem this should raise error
                // up
                //  and let upper layer restore connection and start over (SASL auth, Capabilities,
                //  etc..).
            }

            return;
        }

        log_info("received line: '{}'", line);

        // keep receiving lines
        imap_socket->async_receive_line(
            [&async_receive_line_cb](std::error_code ec, imap_response_line_t line) {
                async_receive_line_cb(ec, line);
            });
    };

    imap_socket->async_connect(gmail_imap_uri, "993", [&](std::error_code ec) {
        log_debug("imap connected: {}", ec.message());

        // start receiving lines
        imap_socket->async_receive_line(
            [&async_receive_line_cb](std::error_code ec, imap_response_line_t line) {
                async_receive_line_cb(ec, std::move(line));
            });

        // sending command three times to test how is it going
        imap_socket->async_send_command("123 CAPABILITY\r\n", [&](std::error_code ec) {
            log_debug("async_send_to_server done: {}", ec.message());
            imap_socket->async_send_command("123 CAPABILITY\r\n", [&](std::error_code ec) {
                log_debug("async_send_to_server done: {}", ec.message());
                imap_socket->async_send_command("123 CAPABILITY\r\n", [&](std::error_code ec) {
                    log_debug("async_send_to_server done: {}", ec.message());
                });
            });
        });
    });
    ctx.run();
}

/*
 * Calculate a maximum required buffer length for decoded output based on input size.
 *
 * Return the value as size_t
 */

void base64_encode_decode_test() {
    const std::string example_test = "dGhlcmUgaXMgc29tZSB0ZXN0IGZvciBlbmNvZGluZw==";
    log_info("output_buffer: '{}'",
             utils::base64_naive_encode(utils::base64_naive_decode(example_test)));
}

void parsing_list_flags_test() {
    imap_parser::parse_flags_list(R"imap(\AnyFlag \AnierFlag)imap");
}

void imap_parsing_test() {
    const std::vector<std::string> samples = {
        R"(LIST (\HasNoChildren) "/" "INBOX")",
        R"(LIST (\A1 \A2) "/" "INBOX")",
        R"(LIST (\AnyFlag \AnierFlag) "/" "[Gmail]")",
        R"(LIST (\HasChildren \Noselect) "/" "[Gmail]")",
        R"(LIST (\Flagged \HasNoChildren) "/" "[Gmail]/&BAYENw- &BDcEVgRABD4ERwQ6BD4ETg-")",
        R"(LIST (\HasNoChildren \Important) "/" "[Gmail]/&BBIEMAQ2BDsEOAQyBD4-")",
        R"(LIST (\HasNoChildren \Trash) "/" "[Gmail]/&BBoEPgRIBDgEOg-")",
        R"(LIST (\HasNoChildren \Sent) "/" "[Gmail]/&BB0EMAQ0BFYEQQQ7BDAEPQRW-")",
        R"(LIST (\HasNoChildren \Junk) "/" "[Gmail]/&BCEEPwQwBDw-")",
        R"(LIST (\All \HasNoChildren) "/" "[Gmail]/&BCMEQQRP- &BD8EPgRIBEIEMA-")",
        R"(LIST (\Drafts \HasNoChildren) "/" "[Gmail]/&BCcENQRABD0ENQRCBDoEOA-")",
    };

    // TODO: questions: what guaranteed should parser provide?
    //      can we expect that hiererchy delimited is always there? I think no, and I guess NIL is
    //      exavtly for this
    // purpose in the protocol. So it should rather be optional. But empty string looks good as
    // well.
    //
    for (auto& s : samples) {
        auto parsed_line_or_err = imap_parser::parse_list_response_line(s);
        if (!parsed_line_or_err) {
            log_error("failed parsing line: '{}': {}", s, parsed_line_or_err.error());
        }
        auto& parsed_line = *parsed_line_or_err;

        log_info("> parsed: {}\n", imap_parser::to_json(parsed_line));

        log_info("parsed box: {}",
                 imap_parser::utils::decode_mailbox_path_from_list_response(parsed_line));

        // check if we can decode with delimiter
    }
}

int main() {
    // gmail_auth_test();
    // imap_parsing_test();
    // parsing_list_flags_test();
}
