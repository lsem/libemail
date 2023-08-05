#include "imap_client.hpp"
#include <b64/naive.h>
#include <map>
#include "imap_socket.hpp"

namespace emailkit {

namespace {
class imap_client_impl_t : public imap_client_t {
   public:
    explicit imap_client_impl_t(asio::io_context& ctx) : m_ctx(ctx) {}

    bool initialize() {
        m_imap_socket = make_imap_socket(m_ctx);
        if (!m_imap_socket) {
            log_info("make_imap_socket failed");
            return false;
        }
        return true;
    }

    virtual void start() override {
        // what does start mean?
        //  I think it should set proper state and then work according to given state and desired
        //  state.
        // so when started, client wants to be connected to the server.
        // but only if it has new data.

        // ideally client should not be that smart and we should have some upper level logic
        // implementing smart things.
    }

    virtual void on_state_change(std::function<void(imap_client_state)> cb) override {
        m_state_change_cb = std::move(cb);
    }

    virtual void async_connect(std::string host,
                               std::string port,
                               async_callback<void> cb) override {
        assert(m_imap_socket);
        m_imap_socket->async_connect(host, port,
                                     [this, cb = std::move(cb)](std::error_code ec) mutable {
                                         if (!ec) {
                                             // just after connect we should start receiving data
                                             // from the server, just in case it decides to
                                             // disconnect us we should at least be able to write
                                             // this fact into the log.
                                             //  alterntive is to start on special command like
                                             //  ready_to_receive()
                                             // TODO: have separatec call ready_receive/go_live
                                             // recive_next_line();
                                         }
                                         cb(ec);
                                     });
    }

    void recive_next_line() {
        log_debug("waiting next line ...");
        m_imap_socket->async_receive_line([this](std::error_code ec, std::string line) {
            if (ec) {
                if (ec == asio::error::eof) {
                    // socket closed
                    // TODO:
                    log_warning("socket closed");
                } else {
                    log_error("error reading line: {}", ec);
                }
                return;
            }
            log_debug("received line: {}", line);

            for (auto& [id, h] : m_active_commands) {
                if (line.find(id) == 0) {
                    log_info("found active command!");
                    auto cb = std::move(h.cb);
                    m_active_commands.erase(id);
                    cb(std::error_code(), line);
                    break;
                }
            }

            recive_next_line();
        });
    }

    // void async_execute_imap_command(std::string command, strong_callback<std::vector<std::)

    virtual void async_obtain_capabilities(async_callback<std::vector<std::string>> cb) override {
        // IDEA: before sending command we register handler, or we can attach handler to
        // send_command itself. through some helper. execute_command(new_command_id(), "CAPABILITY",
        // [](std::string ) { response; }) we can add additional timeout (e.g. 5s). If command comes
        // without ID then we have unregistered response/upstream command.
        //
        const auto id = new_command_id();
        // m_active_commands[id] = {
        //     .cb = [id, cb2 = std::move(cb2)](std::error_code ec, std::string line) mutable {
        //         log_debug("command {} finished: {}: {}", id, ec.message(), line);
        //         // TODO: parse capabilities.
        //         cb2(std::error_code(), std::vector<std::string>{});
        //     }};

        m_imap_socket->async_send_command(
            fmt::format("{} CAPABILITY\r\n", id),
            [this, cb = std::move(cb)](std::error_code ec) mutable {
                if (ec) {
                    // TODO: better message
                    log_error("send caps command failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                m_imap_socket->async_receive_line(
                    [this, cb = std::move(cb)](std::error_code ec, std::string line) mutable {
                        if (ec) {
                            log_error("failed receiving line: {}", ec);
                            cb(ec, {});
                            return;
                        }

                        log_debug("got response: {}", line);

                        // TODO: keep receiving lines until we have OK or NO (check RFC for more).
                        m_imap_socket->async_receive_line(
                            [this, cb = std::move(cb)](std::error_code ec,
                                                       std::string line) mutable {
                                if (ec) {
                                    log_error("failed receiving line2: {}", ec);
                                    cb(ec, {});
                                    return;
                                }

                                log_debug("got response2: {}", line);

                                m_imap_socket->async_receive_line(
                                    [this, cb = std::move(cb)](std::error_code ec,
                                                               std::string line) mutable {
                                        if (ec) {
                                            log_error("failed receiving line3: {}", ec);
                                            cb(ec, {});
                                            return;
                                        }

                                        log_debug("got response3: {}", line);

                                        cb(ec, {});
                                    });
                            });
                    });
            });
    }

    virtual void async_authenticate(xoauth2_creds_t creds, async_callback<void> cb) override {
        // TODO: run capabilities command first, or if this considered to be low-level command
        // then accept capabilities and check there is SASL-IR which is what we are going to support
        // in first implementation.
        // For now we just assume that server supoorts SASL-IR.

        // prepare XOAUTH2 request
        // https://developers.google.com/gmail/imap/xoauth2-protocol
        // https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth
        // const std::string xoauth2_req =
        //     fmt::format("user={}Aauth=Bearer {}^A^A", creds.user_email, creds.oauth_token);

        auto decoded = b64::base64_naive_decode(
            "dXNlcj1zb21ldXNlckBleGFtcGxlLmNvbQFhdXRoPUJlYXJlciB5YTI5LnZGOWRmdDRxbVRjMk52YjNSbGNrQm"
            "hkSFJoZG1semRHRXVZMjl0Q2cBAQ==");
        log_debug("chars:");
        for (char c : decoded) {
            // log_debug("{}: {}", (char)c, (int)c);
        }

        std::string xoauth2_req;
        xoauth2_req += "user=" + creds.user_email;
        xoauth2_req += 0x01;
        xoauth2_req += "auth=Bearer " + creds.oauth_token;
        xoauth2_req += 0x01;
        xoauth2_req += 0x01;

        log_debug("my chars");
        for (char c : xoauth2_req) {
            // log_debug("{}: {}", (char)c, (int)c);
        }

        log_debug("xoauth2_req: {}", xoauth2_req);
        const std::string xoauth2_req_encoded = b64::base64_naive_encode(xoauth2_req);

        // TODO: don't use this.
        const auto id = new_command_id();

        // TODO: this in fact should be stateful thing implement as unit.

        m_imap_socket->async_send_command(
            fmt::format("{} AUTHENTICATE XOAUTH2 {}\r\n", id, xoauth2_req_encoded),
            [this, id, cb = std::move(cb)](std::error_code ec) mutable {
                PROPAGATE_ERROR_VIA_CB(ec, "send AUTHENTICATE XOAUTH2", cb);

                log_debug("auth command send, waiting response");
                
                m_imap_socket->async_receive_line(
                    [this, id, cb = std::move(cb)](std::error_code ec, std::string line) mutable {
                        PROPAGATE_ERROR_VIA_CB(ec, "receive first line", cb);

                        if (line.find(id) == 0) {
                            log_debug("line {} must be a response we were looking for", line);                            
                        }
                    });

                cb({});
            });
    }

    std::string new_command_id() { return std::string("A") + std::to_string(m_command_counter++); }

   private:
    asio::io_context& m_ctx;
    std::shared_ptr<imap_socket_t> m_imap_socket;
    std::function<void(imap_client_state)> m_state_change_cb = [](auto s) {};
    int m_command_counter = 0;

    // a registry of pending commands for which we are waiting response.
    struct pending_command_ctx {
        async_callback<std::string> cb;
    };
    std::map<std::string, pending_command_ctx> m_active_commands;
};

}  // namespace

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx) {
    auto client = std::make_shared<imap_client_impl_t>(ctx);
    if (!client->initialize()) {
        return nullptr;
    }
    return client;
}

}  // namespace emailkit