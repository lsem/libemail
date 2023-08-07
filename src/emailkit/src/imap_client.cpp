#include "imap_client.hpp"
#include <map>
#include "imap_socket.hpp"
#include "utils.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

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
        m_imap_socket->set_option(imap_socket_opts::dump_stream_to_file{});
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
        m_imap_socket->async_receive_line([this](std::error_code ec, imap_response_line_t line) {
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
                    [this, cb = std::move(cb)](std::error_code ec,
                                               imap_response_line_t line) mutable {
                        if (ec) {
                            log_error("failed receiving line: {}", ec);
                            cb(ec, {});
                            return;
                        }

                        log_debug("got response: {}", line);

                        // TODO: keep receiving lines until we have OK or NO (check RFC for more).
                        m_imap_socket->async_receive_line(
                            [this, cb = std::move(cb)](std::error_code ec,
                                                       imap_response_line_t line) mutable {
                                if (ec) {
                                    log_error("failed receiving line2: {}", ec);
                                    cb(ec, {});
                                    return;
                                }

                                log_debug("got response2: {}", line);

                                m_imap_socket->async_receive_line(
                                    [this, cb = std::move(cb)](std::error_code ec,
                                                               imap_response_line_t line) mutable {
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

    struct xoauth2_auth_result_state {
        bool auth_success = false;
        std::string error_details;

        // TODO: collect received untagged lines
        // std::vector<std::string> untagged_lines;
    };

    void receive_xoauth2_result(xoauth2_auth_result_state state,
                                async_callback<xoauth2_auth_result_state> cb) {
        m_imap_socket->async_receive_line([this, state, cb = std::move(cb)](
                                              std::error_code ec,
                                              imap_response_line_t line) mutable {
            if (ec) {
                // TODO: check for eof?
                log_error("async_receive_line failed: {}", ec);
                cb(ec, state);
                return;
            }

            if (line.is_untagged_reply()) {
                // so far just ignore this and keep reading..
                receive_xoauth2_result(state, std::move(cb));
            } else if (line.is_command_continiation_request()) {
                // this must be error happened and server challanged us to accept result
                // and confirm by sending \r\n before it will send its final result (<TAG>
                // OK/NO/BAD)
                log_debug("got continuation request, line: {}", line);

                if (line.tokens.size() > 1) {
                    state.error_details = utils::base64_naive_decode(std::string{line.tokens[1]});
                }

                m_imap_socket->async_send_command(
                    "\r\n", [this, cb = std::move(cb), state](std::error_code ec) mutable {
                        if (ec) {
                            log_error("failed sending \r\n in response to auth challange: {}", ec);
                            cb(ec, state);
                            return;
                        }
                        // sent, we can continue reading and don't expect continuations any more
                        // TODO: put flag in a state that continuations are not expected anymore.
                        receive_xoauth2_result(state, std::move(cb));
                    });
            } else if (line.maybe_tagged_reply()) {
                log_debug("got tagged reply, finishing: line: '{}', line tokens: {}", line,
                          line.tokens);
                if (line.tokens[1] == "OK") {
                    state.auth_success = true;
                } else if (line.tokens[1] == "NO" || line.tokens[1] == "BAD") {
                    state.auth_success = false;
                }
                cb({}, state);
            }
        });
    }

    virtual void async_authenticate(xoauth2_creds_t creds,
                                    async_callback<auth_error_details_t> cb) override {
        // https://developers.google.com/gmail/imap/xoauth2-protocol
        // https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth
        // TODO: run capabilities command first, or if this considered to be low-level command

        std::string xoauth2_req;
        xoauth2_req += "user=" + creds.user_email;
        xoauth2_req += 0x01;
        xoauth2_req += "auth=Bearer " + creds.oauth_token;
        xoauth2_req += 0x01;
        xoauth2_req += 0x01;

        const std::string xoauth2_req_encoded = utils::base64_naive_encode(xoauth2_req);
        auto decoded = utils::base64_naive_decode(xoauth2_req_encoded);

        const auto id = new_command_id();

        m_imap_socket->async_send_command(
            fmt::format("{} AUTHENTICATE XOAUTH2 {}\r\n", id, xoauth2_req_encoded),
            [this, id, cb = std::move(cb)](std::error_code ec) mutable {
                if (ec) {
                    log_error("send AUTHENTICATE XOAUTH2 failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                receive_xoauth2_result(
                    {}, [this, cb = std::move(cb)](std::error_code ec,
                                                   xoauth2_auth_result_state state) mutable {
                        if (ec) {
                            log_error("receive_xoauth2_result failed: {}", ec);
                            cb(ec, {});
                            return;
                        }

                        if (state.auth_success) {
                            cb({}, {});
                        } else {
                            cb(make_error_code(std::errc::protocol_error),
                               auth_error_details_t{.summary = state.error_details});
                        }
                    });
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