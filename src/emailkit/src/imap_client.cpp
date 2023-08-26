#include "imap_client.hpp"
#include "imap_socket.hpp"
#include "utils.hpp"

#include "imap_parser.hpp"
#include "imap_parser_utils.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <map>

namespace emailkit::imap_client {

namespace imap_commands {
std::string encode_cmd(const fetch_t& cmd) {
    const auto second_arg =
        std::visit(overload{[&](fetch_macro m) -> std::string {
                                switch (m) {
                                    case fetch_macro::all:
                                        return "ALL";
                                    case fetch_macro::fast:
                                        return "FAST";
                                    case fetch_macro::full:
                                        return "FULL";
                                    case fetch_macro::RFC822:
                                        return "RFC822";
                                    default:
                                        return "<INVALID>";
                                }
                            },
                            [&](const std::vector<std::string>& attrs) { return ""s; }},
                   cmd.data_item_names_or_macro);

    return fmt::format("fetch {} {}", cmd.sequence_set, second_arg);
}

}  // namespace imap_commands

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

    struct imap_response_t {
        std::string raw_response_bytes;
        std::vector<imap_response_line_t> lines;
        std::string tag;
    };

    virtual void async_execute_simple_command(std::string command,
                                              async_callback<imap_response_t> cb) {
        const auto tag = next_tag();
        m_imap_socket->async_send_command(
            fmt::format("{} {}\r\n", tag, command),
            [this, tag, cb = std::move(cb)](std::error_code ec) mutable {
                imap_response_t response{.tag = tag};
                if (ec) {
                    log_error("failed sending namespace command: {}", ec);
                    cb(ec, std::move(response));
                    return;
                }

                async_receive_response(*m_imap_socket, std::move(response),
                                       [this, cb = std::move(cb)](
                                           std::error_code ec, imap_response_t response) mutable {
                                           if (ec) {
                                               cb(ec, std::move(response));
                                               return;
                                           }

                                           log_debug("{} command finished, lines are:",
                                                     response.tag);
                                           for (auto& line : response.lines) {
                                               log_debug("{}", line);
                                           }

                                           cb({}, std::move(response));
                                       });
            });
    }

    virtual void async_execute_raw_command(std::string command, async_callback<std::string> cb) {
        const auto tag = next_tag();
        m_imap_socket->async_send_command(
            fmt::format("{} {}\r\n", tag, command),
            [this, tag, cb = std::move(cb)](std::error_code ec) mutable {
                if (ec) {
                    log_error("failed sending namespace command: {}", ec);
                    cb(ec, {});
                    return;
                }

                async_receive_response_until_tagged_line(
                    *m_imap_socket, tag, ""s,
                    [this, tag, cb = std::move(cb)](std::error_code ec,
                                                    std::string response) mutable {
                        if (ec) {
                            cb(ec, std::move(response));
                            return;
                        }

                        log_debug("{} command finished, text is: '{}'", tag,
                                  utils::escape_ctrl(response));
                        cb({}, std::move(response));
                    });
            });
    }

    virtual void async_execute_command(imap_commands::namespace_t,
                                       async_callback<void> cb) override {
        // TODO: ensure connected and authenticated?
        async_execute_simple_command(
            "namespace",
            [cb = std::move(cb)](std::error_code ec, imap_response_t response) mutable {
                // TODO: parse response
                cb(ec);
            });
    }

    virtual void async_execute_command(imap_commands::list_t cmd,
                                       async_callback<types::list_response_t> cb) override {
        // https://datatracker.ietf.org/doc/html/rfc3501#section-6.3.8
        // TODO: laternatively we can use parser all the way down instead of this.

        // if (cmd.reference_name.empty()) {
        //     cmd.reference_name = "\"\"";
        // }
        // if (cmd.mailbox_name.empty()) {
        //     cmd.mailbox_name = "\"\"";
        // }
        async_execute_simple_command(
            fmt::format("list \"{}\" \"{}\"", cmd.reference_name, cmd.mailbox_name),
            [cb = std::move(cb)](std::error_code ec, imap_response_t response) mutable {
                if (ec) {
                    log_error("async_execute_simple_command failed: {}", ec);
                    cb(ec, {});
                    return;
                }
                // TODO: what about tags and generic failures from server (bye)? CHECK THE GRAMMAR
                // and write unit tests.

                types::list_response_t command_result;
                for (auto& l : response.lines) {
                    if (l.is_untagged_reply()) {
                        log_debug("untagged reply, stripping *");

                        auto unwrapped_line = l.unwrap_untagged_reply();

                        auto parsed_line_or_err =
                            imap_parser::parse_list_response_line(unwrapped_line);
                        if (!parsed_line_or_err) {
                            log_error("failed parsing line: '{}': {}", unwrapped_line,
                                      parsed_line_or_err.error());
                            // TODO: I would rather liked to have strict mode to break it and
                            // control it from settings. Can be helpful for QA.
                            continue;
                        }
                        auto& parsed_line = *parsed_line_or_err;

                        log_debug("parsed_line.mailbox: '{}'", parsed_line.mailbox);
                        log_debug("parsed_line.hierarchy_delimiter: '{}'",
                                  parsed_line.hierarchy_delimiter);

                        command_result.inbox_list.emplace_back(types::list_response_entry_t{
                            .mailbox_raw = parsed_line.mailbox,
                            .inbox_path =
                                imap_parser::utils::decode_mailbox_path_from_list_response(
                                    parsed_line),
                            .flags = parsed_line.mailbox_list_flags});
                    } else if (l.maybe_tagged_reply()) {
                        // this must be reply to our command, this should be guaranteed by
                        // execute_simple_command
                        if (l.is_ok_response()) {
                            cb(ec, std::move(command_result));
                            return;
                        } else if (l.is_bad_response()) {
                            cb(make_error_code(types::imap_errors::imap_bad), {});
                            return;
                        } else if (l.is_no_response()) {
                            cb(make_error_code(types::imap_errors::imap_no), {});
                            return;
                        }
                    } else {
                        log_error("not implemented!: '{}'", l.line);
                    }
                }

                log_warning("no lines from LIST command");
                cb(make_error_code(std::errc::no_message_available), {});
            });
    }

    virtual void async_execute_command(imap_commands::select_t cmd,
                                       async_callback<types::select_response_t> cb) override {
        async_execute_simple_command(
            fmt::format("select {}", cmd.mailbox_name),
            [cb = std::move(cb)](std::error_code ec, imap_response_t imap_resp) mutable {
                if (ec) {
                    log_error("async_execute_simple_command failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                // Parsing response
                types::select_response_t select_resp;
                auto records_or_err =
                    imap_parser::parse_mailbox_data_records(imap_resp.raw_response_bytes);
                if (!records_or_err) {
                    // TODO: this needs to be somehow delivered to develoers alongside with logs for
                    // anallysis.
                    log_error("failed parsing imap response");
                    cb(records_or_err.error(), {});
                    return;
                }
                auto& records = *records_or_err;
                for (auto& rec : records) {
                    std::visit(overload{[&](const imap_parser::flags_mailbox_data_t& v) {
                                            select_resp.flags = std::move(v.flags_vec);
                                        },
                                        [&](const imap_parser::permanent_flags_mailbox_data_t& v) {
                                            select_resp.permanent_flags = std::move(v.flags_vec);
                                        },
                                        [&](const imap_parser::uidvalidity_data_t& v) {
                                            select_resp.uid_validity = v.value;
                                        },
                                        [&](const imap_parser::exists_mailbox_data_t& v) {
                                            select_resp.exists = v.value;
                                        },
                                        [&](const imap_parser::recent_mailbox_data_t& v) {
                                            select_resp.recents = v.value;
                                        },
                                        [&](const imap_parser::unseen_resp_text_code_t& v) {
                                            select_resp.opt_unseen = v.value;
                                        },
                                        [&](const imap_parser::uidnext_resp_text_code_t& v) {
                                            select_resp.uid_next = v.value;
                                        },
                                        [&](const imap_parser::read_write_resp_text_code_t& v) {
                                            select_resp.read_write_mode =
                                                types::read_write_mode_t::read_write;
                                        },
                                        [&](const imap_parser::read_only_resp_text_code_t& v) {
                                            select_resp.read_write_mode =
                                                types::read_write_mode_t::read_only;
                                        },
                                        [&](const imap_parser::try_create_resp_text_code_t& v) {
                                            select_resp.read_write_mode =
                                                types::read_write_mode_t::try_create;
                                        }},
                               rec);
                }

                // TODO: validate?

                cb({}, std::move(select_resp));
            });
    }

    virtual void async_execute_command(imap_commands::fetch_t cmd,
                                       async_callback<types::fetch_response_t> cb) override {
        async_execute_raw_command(
            encode_cmd(cmd),
            [cb = std::move(cb)](std::error_code ec, std::string imap_resp) mutable {
                if (ec) {
                    log_error("async_execute_simple_command failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                auto message_data_or_err = imap_parser::parse_message_data(imap_resp);
                if (!message_data_or_err) {
                    log_error("failed parsing message data: {}", message_data_or_err.error());
                    return;
                }

                log_warning("PARSING SUCCESSFUL!");

                cb({}, {});
            });
    }

    // read input line by line until tagged line received. Returns raw, unparsed bytes.
    void async_receive_response_until_tagged_line(imap_socket_t& socket,
                                                  std::string tag,
                                                  std::string curr_response,
                                                  async_callback<std::string> cb) {
        socket.async_receive_raw_line([this, &socket, tag = std::move(tag), cb = std::move(cb),
                                       curr_response = std::move(curr_response)](
                                          std::error_code ec, std::string line) mutable {
            if (ec) {
                log_error("failed receiving next raw line: {}", ec);
                cb(ec, {});
                return;
            }

            log_debug("received raw line");

            // check if line is tagged response (TODO: do it right, with parser or make sure it is
            // correct according to the grammar)
            bool stop_reading = false;
            if (line.rfind(tag, 0) == 0) {
                log_debug("got tagged reply, stop reading..");
                stop_reading = true;
            }

            curr_response += std::move(line);

            if (stop_reading) {
                cb({}, std::move(curr_response));
            } else {
                async_receive_response_until_tagged_line(socket, std::move(tag),
                                                         std::move(curr_response), std::move(cb));
            }
        });
    }

    void async_receive_response(imap_socket_t& socket,
                                imap_response_t r,
                                async_callback<imap_response_t> cb) {
        // receiving response means we read until we get tag or +. If we encounter plus.
        socket.async_receive_line([this, &socket, cb = std::move(cb), r = std::move(r)](
                                      std::error_code ec, imap_response_line_t line) mutable {
            if (ec) {
                log_error("failed receiving line {}: {}", r.lines.size() + 1, ec);
                cb(ec, std::move(r));
                return;
            }

            r.raw_response_bytes += line.line;
            r.lines.emplace_back(std::move(line));
            const auto& l = r.lines.back();

            if (l.first_token_is(r.tag)) {
                cb({}, std::move(r));
                return;
            } else if (l.is_command_continiation_request()) {
                cb(make_error_code(std::errc::interrupted), std::move(r));
                return;
            } else if (l.is_untagged_reply()) {
                // continue receiving lines
                async_receive_response(socket, std::move(r), std::move(cb));
                return;
            } else {
                log_error("unexpected line from server: {}", l);
                cb(make_error_code(std::errc::bad_message), std::move(r));
                return;
            }
        });
    }

    std::string new_command_id() { return std::string("A") + std::to_string(m_command_counter++); }
    std::string next_tag() { return new_command_id(); }

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
};  // namespace

}  // namespace

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx) {
    auto client = std::make_shared<imap_client_impl_t>(ctx);
    if (!client->initialize()) {
        return nullptr;
    }
    return client;
}

}  // namespace emailkit::imap_client