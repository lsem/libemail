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

    // void recive_next_line() {
    //     log_debug("waiting next line ...");
    //     m_imap_socket->async_receive_line([this](std::error_code ec, imap_response_line_t line) {
    //         if (ec) {
    //             if (ec == asio::error::eof) {
    //                 // socket closed
    //                 // TODO:
    //                 log_warning("socket closed");
    //             } else {
    //                 log_error("error reading line: {}", ec);
    //             }
    //             return;
    //         }
    //         log_debug("received line: {}", line);

    //         for (auto& [id, h] : m_active_commands) {
    //             if (line.find(id) == 0) {
    //                 log_info("found active command!");
    //                 auto cb = std::move(h.cb);
    //                 m_active_commands.erase(id);
    //                 cb(std::error_code(), line);
    //                 break;
    //             }
    //         }

    //         recive_next_line();
    //     });
    // }

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

    virtual void async_authenticate(xoauth2_creds_t creds,
                                    async_callback<auth_error_details_t> cb) override {
        // TODO: run capabilities command first, or if this considered to be low-level command
        // then accept capabilities and check there is SASL-IR which is what we are going to support
        // in first implementation.
        // For now we just assume that server supoorts SASL-IR.
        // https://www.rfc-editor.org/rfc/rfc4959

        // '*' in IMAP: https://datatracker.ietf.org/doc/html/rfc3501#section-2.2.2
        // '+' in IMAP: https://datatracker.ietf.org/doc/html/rfc3501#section-2.2.1

        // prepare XOAUTH2 request
        // https://developers.google.com/gmail/imap/xoauth2-protocol
        // https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth
        // const std::string xoauth2_req =
        //     fmt::format("user={}Aauth=Bearer {}^A^A", creds.user_email, creds.oauth_token);

        log_info("creds.user_email: {}", creds.user_email);

        std::string xoauth2_req;
        xoauth2_req += "user=" + creds.user_email;
        xoauth2_req += 0x01;
        xoauth2_req += "auth=Bearer " + creds.oauth_token;
        xoauth2_req += 0x01;
        xoauth2_req += 0x01;

        const std::string xoauth2_req_encoded = utils::base64_naive_encode(xoauth2_req);
        auto decoded = utils::base64_naive_decode(xoauth2_req_encoded);

        const auto id = new_command_id();

        // TODO: this in fact should be stateful thing implement as unit.
        // TODO: don't use this.

        m_imap_socket->async_send_command(
            fmt::format("{} AUTHENTICATE XOAUTH2 {}\r\n", id, xoauth2_req_encoded),
            [this, id, cb = std::move(cb)](std::error_code ec) mutable {
                PROPAGATE_ERROR_VIA_CB(ec, "send AUTHENTICATE XOAUTH2", cb);

                log_debug("auth command send, waiting response");

                struct context_t {
                    std::string base64_error_challenge;
                    bool auth_success = false;
                };
                auto shared_ctx = std::make_shared<context_t>();

                // keep receiving lines until
                // TODO: timeout or putting timeout on entire high-level procedure
                async_keep_receiving_lines_until(
                    m_imap_socket,
                    [shared_ctx, id](const imap_response_line_t& line) -> std::error_code {
                        log_info("received line: '{}'", line);
                        if (line.is_untagged_reply()) {
                            // this is so called untagged response and indicates data transmitted
                            // from server which do not indicate command completion, we should not
                            // take any action and continue.
                            log_debug("starts with *, skip");
                            return {};
                        } else if (line.is_command_continiation_request()) {
                            // the server indicates that it is ready for remainder of the command,
                            // in other words it is not going to send us more data (as with "*"")
                            // and it is our turn
                            shared_ctx->base64_error_challenge = line.line;
                            return make_error_code(std::errc::interrupted);
                        } else if (line.tokens.size() > 0 && line.tokens[0] == id) {
                            shared_ctx->base64_error_challenge = line.line;
                            shared_ctx->auth_success = true;
                            return make_error_code(std::errc::interrupted);
                        }
                        return {};
                    },
                    [this, cb = std::move(cb), id, shared_ctx](std::error_code ec) mutable {
                        if (ec && ec != std::errc::interrupted) {
                            PROPAGATE_ERROR_VIA_CB(ec, "async_keep_receiving_lines_until/xoauth2",
                                                   cb);
                        }

                        if (ec == std::errc::interrupted) {
                            if (shared_ctx->auth_success) {
                                log_debug("authenticated! line was: '{}'",
                                          utils::replace_control_chars(
                                              shared_ctx->base64_error_challenge));
                                // TODO: we need to parse and make sure we authenticated exactly the
                                // same user.
                                // TODO: check RFC.
                                cb({}, {});
                                return;
                            }
                            // server responded with error code and we need to send CRLF
                            log_debug("server responded with challenge: '{}'",
                                      shared_ctx->base64_error_challenge);

                            shared_ctx->base64_error_challenge = std::string(
                                shared_ctx->base64_error_challenge, 2);  // left+strip "+ "
                            // this is supposed to be encoded JSON with error
                            const auto maybe_json_error =
                                utils::base64_naive_decode(shared_ctx->base64_error_challenge);

                            // TODO: is JSON here google specific or generic XOATUH2?
                            rapidjson::Document d;
                            d.Parse(maybe_json_error.c_str());
                            if (d.HasParseError()) {
                                // no, it is not a json
                                // TODO: warning: it is unsafe print garbage!
                                log_warning("returned response is not a json: '{}'",
                                            maybe_json_error);
                                cb(make_error_code(std::errc::protocol_error),
                                   {.summary = "error details is not a JSON"});
                                return;
                            }

                            auto as_str_or = [&d](const char* key,
                                                  std::string _default = {}) -> std::string {
                                return d.HasMember(key) && d[key].IsString() ? d[key].GetString()
                                                                             : _default;
                            };
                            auto as_int_or = [&d](const char* key, int _default = {}) -> int {
                                return d.HasMember(key) && d[key].IsInt() ? d[key].GetInt()
                                                                          : _default;
                            };

                            log_info("it is a JSON: {}", maybe_json_error);
                            auto status = as_str_or("status");
                            log_warning("server returned: {}", status);

                            // once we received this line explaining error we should send \r\n to
                            // finish auth session.
                            m_imap_socket->async_send_command("\r\n", [this, id, cb = std::move(cb),
                                                                       status, maybe_json_error](
                                                                          std::error_code
                                                                              ec) mutable {
                                PROPAGATE_ERROR_VIA_CB(ec, "send \\r\\n after error", cb);

                                // TODO: alternatively we can read everything until we find
                                // response to the command, just in case server wants to write
                                // more *.
                                m_imap_socket->async_receive_line(
                                    [cb = std::move(cb), id, status, maybe_json_error](
                                        std::error_code ec, imap_response_line_t line) mutable {
                                        PROPAGATE_ERROR_VIA_CB(ec, "receive command response line",
                                                               cb);

                                        log_debug("got command response line: '{}'", line);

                                        if (line.tokens.size() > 0 && line.tokens[0] == id) {
                                            log_debug("AUTH command finished");
                                            // TODO: analyze whether it is really "BAD"!
                                            const auto response = std::string(line.line, id.size() + 1);
                                            if (line.tokens[1] == "BAD") {
                                                std::string sasl_fail = std::string(response, 4);
                                                cb(make_error_code(std::errc::protocol_error),
                                                   {.summary = fmt::format(
                                                        "SASL error: {}, server status: {}",
                                                        sasl_fail, status)});
                                            } else if (line.tokens[1] == "OK") {
                                                log_warning("OK returned after error details ");
                                                cb({}, {});
                                            } else if (line.tokens[1] == "NO") {
                                                // TODO: we must  have some good details!
                                                cb(make_error_code(std::errc::invalid_argument),
                                                   {.summary = maybe_json_error});
                                            }
                                        } else {
                                            log_warning("received something else, giving up");
                                            cb(make_error_code(std::errc::protocol_not_supported),
                                               {.summary = fmt::format(
                                                    "AUTH command not finished, server status: {}",
                                                    status)});
                                        }
                                    });
                            });

                            // TODO: provide details to the user so that he or she can ask support
                            // for this problem.

                            // as_str_or("")
                        }

                        log_debug("received lines!");
                    });

                // m_imap_socket->async_receive_line(
                //     [this, id, cb = std::move(cb)](std::error_code ec, std::string line) mutable
                //     {
                //         log_debug("got response: {}: {}", ec, line);
                //         PROPAGATE_ERROR_VIA_CB(ec, "receive first line", cb);

                //         // TODO: use to unit test: "* OK Gimap ready for requests from
                //         149.255.130.5 f19mb79827071wmq\r\n"

                //         if (line.find(id) == 0) {
                //             log_debug("line {} must be a response we were looking for", line);
                //         }

                //         cb({});
                //     });
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