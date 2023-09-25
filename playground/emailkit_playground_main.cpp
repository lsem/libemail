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

namespace emailapp {
// what is emailapp?
// lets think about it..
// app has its state dependent on which it presents different UI. E.g. when we have no configuration
// we cannot do anything but need to login into the server first. now, imagine we are logged in and
// have a connection. We should connect to the server first and then once we are connected, we can
// do some backeground work and/or process actions from the user.

// How we are supposed to organize an app that has different states? One way is to use variant
// again: app_state = variant<Loggin_required, Working>; class Working {
//      tree;
//      email_list;
//      email_preview;
// }
// The rendered for interface is should listen for updates from application and be ready to present
// what the application needs.

class EmailAppListener {
   public:
    virtual void EmailAppListener__on_statate_changed() = 0;
};

// so the app when started just checks if there is login information and starts if there is.
// once being started, it should porbably somehow check what is current status of current cache (if
// there is any). if there is no cache at all, the app lists some emails.

class EmailApp {
   public:
    explicit EmailApp(asio::io_context& ctx) : m_ctx(ctx), m_imap_client() {}
    bool initialize() {
        m_imap_client = emailkit::imap_client::make_imap_client(m_ctx);
        if (!m_imap_client) {
            log_error("failed creating imap client");
            return false;
        }

        m_google_auth = emailkit::make_google_auth(m_ctx, "127.0.0.1", "8089");
        if (!m_google_auth) {
            log_error("failed creating google auth");
            return false;
        }
        return true;
    }

    void async_run(async_callback<void> cb) {
        // initialize
        async_authenticate([cb = std::move(cb)](std::error_code ec) mutable {
            if (ec) {
                log_error("failed to authenticate: {}", ec);
                // TODO: what we are supposed to do in this case?
                // I guess we should switch interface to authentication.
                // But it would be better if we can handle specific error. What if we just don't
                // have an internet and that is why we cannot authenticate.
                std::abort();
            }

            cb({});

            // So we authenticated, what next?
        });
    }

    void async_authenticate(async_callback<void> cb) {
        const auto app_creds = emailkit::google_auth_app_creds_t{
            .client_id = "303173870696-bsun94hmoseeumiat4iaa6dr752ta805.apps.googleusercontent.com",
            .client_secret = "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"};
        const std::vector<std::string> scopes = {"https://mail.google.com"};

        log_info("authentication in Google");

        m_google_auth->async_handle_auth(
            app_creds, scopes,
            [this, cb = std::move(cb)](std::error_code ec, auth_data_t auth_data) mutable {
                if (ec) {
                    log_error("async_handle_auth failed: {}", ec);
                    return;
                }
                log_info("authenticated in Google");

                m_imap_client->async_connect(
                    "imap.gmail.com", "993",
                    [this, cb = std::move(cb), auth_data](std::error_code ec) mutable {
                        if (ec) {
                            log_error("failed connecting Gmail IMAP: {}", ec);
                            return;
                        }

                        // TODO: get user email from google itself!
                        const std::string user = "liubomyr.semkiv.test2@gmail.com";

                        // now we need to authenticate on imap server
                        m_imap_client->async_authenticate(
                            {.user_email = user, .oauth_token = auth_data.access_token},
                            [this, cb = std::move(cb)](
                                std::error_code ec,
                                emailkit::imap_client::auth_error_details_t err_details) mutable {
                                if (ec) {
                                    log_error("imap auth failed: {}", ec);
                                    // TODO: how we are supposed to handle this error?
                                    // I gues we shoud raise special error for this case and
                                    // depdening on why auth failed either retry with same creds (if
                                    // there is no Internet), or go to authentication. Or, we should
                                    // somehow allow user to start over.
                                    cb(ec);
                                    return;
                                }

                                log_info("authenticated on IMAP server");
                                // TODO: consider having some kind of connection info state which
                                // would indicate information about all current connections.
                                cb({});
                            });
                    });
            });
    }

    asio::io_context& m_ctx;
    std::shared_ptr<emailkit::imap_client::imap_client_t> m_imap_client;
    std::shared_ptr<emailkit::google_auth_t> m_google_auth;
};

std::shared_ptr<EmailApp> make_email_app(asio::io_context& ctx, EmailAppListener& listener) {
    auto app_instance = std::make_shared<EmailApp>(ctx);
    if (!app_instance->initialize()) {
        return nullptr;
    }
    return app_instance;
}

}  // namespace emailapp

void fetch_one(imap_client::imap_client_t& client, int n) {
    namespace imap_commands = emailkit::imap_client::imap_commands;
    namespace fi = imap_commands::fetch_items;
    client.async_execute_command(
        imap_client::imap_commands::fetch_t{
            .sequence_set = imap_commands::fetch_sequence_spec{.from = n, .to = n},
            .items =
                imap_commands::fetch_items_vec_t{
                    // fi::body_t{},
                    fi::body_structure_t{},
                    // fi::rfc822_header_t{},
                    // fi::envelope_t{},
                    // fi::flags_t{},
                    // fi::internal_date_t{},
                    // fi::rfc822_t{},
                    // fi::rfc822_header_t{},
                    // fi::rfc822_size_t{},
                    // fi::rfc822_text_t{},

                }},
        [&client, n](std::error_code ec, imap_client::types::fetch_response_t r) {
            if (ec) {
                log_error("fetch command failed: {}", ec);
                return;
            }
            log_info("fetch of {} done", n);
        });
}

void fetch_messages_in_a_row(imap_client::imap_client_t& client, int count) {
    namespace imap_commands = emailkit::imap_client::imap_commands;
    namespace fi = imap_commands::fetch_items;

    if (count == 0) {
        log_info("done with rows");
        return;
    }

    client.async_execute_command(
        imap_client::imap_commands::fetch_t{
            .sequence_set = imap_commands::fetch_sequence_spec{.from = count, .to = count},
            .items =
                imap_commands::fetch_items_vec_t{
                    // fi::body_t{},
                    // fi::body_structure_t{},
                    // fi::envelope_t{},
                    // fi::flags_t{},
                    // fi::internal_date_t{},
                    fi::rfc822_t{},
                    fi::rfc822_header_t{},
                    fi::rfc822_size_t{},
                    fi::rfc822_text_t{},

                }},
        [&client, count](std::error_code ec, imap_client::types::fetch_response_t r) {
            if (ec) {
                log_error("fetch command failed: {}", ec);
                return;
            }
            log_info("fetch of {} done", count);

            fetch_messages_in_a_row(client, count - 1);
        });
}

void gmail_fetch_some_messages(imap_client::imap_client_t& client) {
    client.async_execute_command(imap_client::imap_commands::namespace_t{}, [&](std::error_code
                                                                                    ec) {
        if (ec) {
            log_error("failed executing ns command: {}", ec);
            return;
        }

        log_info("executed namespace command");

        client.async_execute_command(
            imap_client::imap_commands::list_t{.reference_name = "", .mailbox_name = "*"},
            [&](std::error_code ec, imap_client::types::list_response_t response) {
                if (ec) {
                    log_error("failed exeucting list command: {}", ec);
                    return;
                }

                log_info("executed list command:\n{}", fmt::join(response.inbox_list, "\n"));
                // for (auto& entry : response.inbox_list) {
                //     log_info("    {}", entry);
                // }

                client.async_execute_command(
                    imap_client::imap_commands::select_t{.mailbox_name = "\"INBOX\""},

                    // imap_client::imap_commands::select_t{.mailbox_name =
                    //                                          "\"[Gmail]/&BCcENQRABD0ENQRCBDoEOA-\""},
                    [&](std::error_code ec, imap_client::types::select_response_t r) {
                        if (ec) {
                            log_error("gmail command failed: {}", ec);
                            return;
                        }

                        log_info(
                            "unseen number in mailbox is: {}, and recent "
                            "number is: {}, in total there is {} emails in the "
                            "box.",
                            r.opt_unseen.value_or(0), r.recents, r.exists);

                        fetch_one(client, 32);

                        namespace imap_commands = emailkit::imap_client::imap_commands;
                        namespace fi = imap_commands::fetch_items;

                        // client.async_execute_command(
                        //     imap_client::imap_commands::fetch_t{
                        //         .sequence_set =
                        //             imap_commands::fetch_sequence_spec{.from = 1, .to = 200},
                        //         // .items = imap_commands::all_t{}
                        //         .items =
                        //             imap_commands::fetch_items_vec_t{
                        //                 fi::body_t{},
                        //                 fi::body_structure_t{},
                        //                 fi::envelope_t{},
                        //                 fi::flags_t{},
                        //                 fi::internal_date_t{},
                        //                 fi::rfc822_t{},
                        //                 fi::rfc822_header_t{},
                        //                 fi::rfc822_size_t{},
                        //                 fi::rfc822_text_t{},
                        //             }},
                        //     [&](std::error_code ec, imap_client::types::fetch_response_t r) {
                        //         if (ec) {
                        //             log_error("fetch command failed: {}", ec);
                        //             return;
                        //         }
                        //         log_info("fetch done");
                        //     });
                    });
            });
    });
}

// body            = "(" (body-type-1part / (1*body SP media-subtype)) ")"

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
    if (!imap_client) {
        log_error("failed creating imap_client");
        return;
    }

    // We are requesting authentication for our app (represented by app_creds) for scoped needed for
    // our application.
    auto google_auth = emailkit::make_google_auth(ctx, "127.0.0.1", "8089");
    google_auth->async_handle_auth(
        app_creds, scopes, [&](std::error_code ec, auth_data_t auth_data) {
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
                //     [&, user, auth_data](std::error_code ec, std::vector<std::string> caps) { //
                //     if (ec) {
                //             log_error("failed otaining capabilities: {}", ec);
                //             return;
                //         }

                imap_client->async_authenticate(
                    {.user_email = user, .oauth_token = auth_data.access_token},
                    [&](std::error_code ec,
                        emailkit::imap_client::auth_error_details_t err_details) {
                        if (ec) {
                            log_error("async_authenticate failed: {}{}", ec,
                                      err_details.summary.empty()
                                          ? ""
                                          : fmt::format(", summary: {})", err_details.summary));
                            return;
                        }

                        log_info("authenticated to gimap");
                        gmail_fetch_some_messages(*imap_client);
                    });
            });
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

static GMimeMessage* parse_message(int fd) {
    GMimeMessage* message;
    GMimeParser* parser;
    GMimeStream* stream;

    log_debug("creating the stream");
    /* create a stream to read from the file descriptor */
    stream = g_mime_stream_fs_new(fd);

    log_debug("creating the parser");
    /* create a new parser object to parse the stream */
    parser = g_mime_parser_new_with_stream(stream);

    log_debug("unreading the stream");
    /* unref the stream (parser owns a ref, so this object does not actually get free'd until we
     * destroy the parser) */
    g_object_unref(stream);

    log_debug("parsing message");
    /* parse the message from the stream */
    message = g_mime_parser_construct_message(parser, NULL);

    log_debug("unrefing parser and tream");
    /* free the parser (and the stream) */
    // g_object_unref (parser);

    return message;
}

#ifdef __linux
#include <sanitizer/lsan_interface.h>
void handler(int signum) {
    __lsan_do_leak_check();
    std::exit(-1);
}
void install_handlers() {
    signal(SIGINT, handler);
}
#else
void install_handlers() {}
#endif

class ConsoleController : public emailapp::EmailAppListener {
   public:
    void EmailAppListener__on_statate_changed() override {
        // ..
    }
};

int main() {
    install_handlers();
    g_mime_init();

    asio::io_context ctx;

    ConsoleController controller;

    auto app = emailapp::make_email_app(ctx, controller);
    if (!app) {
        log_error("failed creating app");
        return -1;
    }
    app->async_run([](std::error_code ec) {
        if (ec) {
            log_error("failed running app: {}", ec);
            return;
        }

        log_info("application is running");
    });

    ctx.run();

    g_mime_shutdown();
}
