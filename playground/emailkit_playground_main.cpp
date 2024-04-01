#include <emailkit/global.hpp>
#include <fmt/ranges.h>
// #include <emailapp-core/emailapp_core.hpp>
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


// class ConsoleContoller : public emailapp::core::EmailAppCoreCallbacks {
//    public:
//     virtual void async_provide_credentials(async_callback<std::vector<emailapp::core::Account>> cb) {
//         // ..
//     }
// };

int main() {
    install_handlers();
    g_mime_init();

    asio::io_context ctx;

    // ConsoleContoller controller;

    // auto app = emailapp::core::make_emailapp_core(ctx, controller);
    // if (!app) {
    //     log_error("failed creating app");
    //     return -1;
    // }
    // log_info("test");
    // app->async_run([](std::error_code ec) {
    //     if (ec) {
    //         log_error("failed running app: {}", ec);
    //         return;
    //     }

    //     log_info("application is running");
    // });

    ctx.run();

    g_mime_shutdown();
}
