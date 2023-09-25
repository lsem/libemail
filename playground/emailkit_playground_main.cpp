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
                    });
            });
    });
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
