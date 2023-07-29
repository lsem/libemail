#include <emailkit/emailkit.hpp>
#include <emailkit/global.hpp>
#include <emailkit/imap_client.hpp>
#include <emailkit/imap_socket.hpp>
#include <emailkit/log.hpp>
#include <iostream>

#include <fmt/ranges.h>

int main() {
    asio::io_context ctx;

    // Sketch of design:
    //    on highest level we are going to have abstraction specific for application: e.g.
    //    desktop_app_v1 which would be responsible for driving things. E.g. there should be an
    //    aspect to keep connection to the server the task is to implement to provide simple and
    //    reliable mechanism for re-connnecting to the server. on this level another thing would be
    //    to watch for token expiration and refresh it. on if it is already expired to handle
    //    re-login interaction with user.
    // on this level we can even have async message bus.
    //
    // there can be one more presentation layer which is responsible for interface states. e.g. when
    // core in bad state or does some recreation/soft-reset we may want to have different UI
    // (deactivate things, change border color).

    // DESIGN IDEA:
    //    we can have a mailbox abstraction which allows to manipulate it and receive notifications.
    //   auto mb = create_mailbox(imap_socket, "INBOX")
    //    mb->on_size_changed([](std::error_code ec, size_t new_size));
    //    mb->on_new_email()
    //  https://datatracker.ietf.org/doc/html/rfc3501#section-5.2
    //
    //

    // Another reasonable abstraction would be SERVER or just server INFO.
    //  once we connected we can get server info and keep it somewhere around to be able to query
    //  for capabilites and have corresponding functionality enabled in our client.

    // auto imap_socket = emailkit::make_imap_socket(ctx);
    // const std::string gmail_imap_uri = "imap.gmail.com";

    // async_callback<std::string> async_receive_line_cb;

    // async_receive_line_cb = [&async_receive_line_cb, &imap_socket](std::error_code ec,
    //                                                                std::string line) {
    //   if (ec) {
    //     if (ec == asio::error::eof) {
    //       log_warning("connection closed by the server (eof)");
    //       // TODO: what should we do?
    //     } else {
    //       // receive error
    //       log_error("imap client run failed: {}", ec.message());
    //       // TODO: reconnect or restart? I guess in realistic problem this should raise error up
    //       //  and let upper layer restore connection and start over (SASL auth, Capabilities,
    //       //  etc..).
    //     }

    //     return;
    //   }

    //   log_info("received line: '{}'", line);

    //   // keep receiving lines
    //   imap_socket->async_receive_line([&async_receive_line_cb](std::error_code ec, std::string
    //   line) {
    //     async_receive_line_cb(ec, line);
    //   });
    // };

    // imap_socket->async_connect(gmail_imap_uri, "993", [&](std::error_code ec) {
    //   log_debug("imap connected: {}", ec.message());

    //   // start receiving lines
    //   imap_socket->async_receive_line([&async_receive_line_cb](std::error_code ec, std::string
    //   line) {
    //     async_receive_line_cb(ec, std::move(line));
    //   });

    //   // sending command three times to test how is it going
    //   imap_socket->async_send_command("123 CAPABILITY\r\n", [&](std::error_code ec) {
    //     log_debug("async_send_to_server done: {}", ec.message());
    //     imap_socket->async_send_command("123 CAPABILITY\r\n", [&](std::error_code ec) {
    //       log_debug("async_send_to_server done: {}", ec.message());
    //       imap_socket->async_send_command("123 CAPABILITY\r\n", [&](std::error_code ec) {
    //         log_debug("async_send_to_server done: {}", ec.message());
    //       });
    //     });
    //   });
    // });

    // TODO: since we are going to recreate client for each new connection we should rather put our
    // settings into constructor.
    auto imap_client = emailkit::make_imap_client(ctx);
    imap_client->async_connect("imap.gmail.com", "993", [&](std::error_code ec) {
        if (ec) {
            log_error("connect failed: {}", ec);
            return;
        }
        imap_client->async_obtain_capabilities(
            [&](std::error_code ec, std::vector<std::string> caps) {
                if (ec) {
                    log_error("obtain capabilities failed: {}", ec);
                    return;
                }

                log_info("caps: {}", caps);
            });
    });
    ctx.run();
}
