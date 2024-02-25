
#include <emailkit/global.hpp>
#include "imap_client.hpp"
#include "imap_socket.hpp"
#include "utils.hpp"

#include "imap_parser.hpp"
#include "imap_parser__rfc822.hpp"
#include "imap_parser_utils.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <map>

namespace emailkit::imap_client {

namespace {
// This actually is not going to be used at all.
const emailkit::types::EmailAddress default_mail_addres{.raw_email_address = "user@example.com"};

expected<void> capture_headers(imap_parser::rfc822::RFC822ParserStateHandle state,
                               emailkit::types::MailboxEmail& mail) {
    if (auto maybe_date = imap_parser::rfc822::get_date(state)) {
        mail.date = *maybe_date;
    } else {
        log_error("no DATE header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (auto maybe_str = imap_parser::rfc822::get_subject(state)) {
        mail.subject = *maybe_str;
    } else {
        log_error("no SUBJECT header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (auto maybe_addr = imap_parser::rfc822::get_from_address(state)) {
        mail.from = *maybe_addr;
    } else {
        log_error("no FROM header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (auto maybe_addr = imap_parser::rfc822::get_to_address(state)) {
        mail.to = *maybe_addr;
    } else {
        log_error("no TO header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (auto maybe_addr = imap_parser::rfc822::get_cc_address(state)) {
        mail.cc = *maybe_addr;
    } else {
        log_error("no CC header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (auto maybe_addr = imap_parser::rfc822::get_bcc_address(state)) {
        mail.bcc = *maybe_addr;
    } else {
        log_error("no BCC header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (auto maybe_addr = imap_parser::rfc822::get_sender_address(state)) {
        mail.sender = *maybe_addr;
    } else {
        // Sender is optional?
        log_error("no SENDER header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    if (auto maybe_addr = imap_parser::rfc822::get_reply_to_address(state)) {
        mail.reply_to = *maybe_addr;
    } else {
        // Sender is optional?
        log_error("no REPLY-TO header in RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    // Collect all other headers. This may be somewhat ineficient to heep all headers locally, but
    // lets see, maybe we can afford it.
    if (auto headers_or_err = imap_parser::rfc822::get_headers(state)) {
        mail.raw_headers = std::move(*headers_or_err);
    } else {
        log_error("failed getting headers for RFC822 message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    // Non mandatory fields.

    if (auto maybe_message_id = imap_parser::rfc822::get_message_id(state)) {
        mail.message_id = *maybe_message_id;
    } else {
        log_warning("no MESSAGE-ID header in RFC822 message");
    }

    if (auto it = mail.raw_headers.find("In-Reply-To"); it != mail.raw_headers.end()) {
        mail.in_reply_to = it->second;
    }

    if (auto it = mail.raw_headers.find("References"); it != mail.raw_headers.end()) {
        // TODO: Check if this SPACE delimiter is the only possible one.
        mail.references = emailkit::utils::split(it->second, ' ');
    }

    return {};
}

using namespace emailkit::imap_parser::wip;
void traverse_body(const Body& body, int indent_width = 0) {
    std::string indent_str = std::string(indent_width, ' ');

    if (std::holds_alternative<std::unique_ptr<BodyType1Part>>(body)) {
        auto& one_part = *std::get<std::unique_ptr<BodyType1Part>>(body);

        if (auto* text_part = std::get_if<BodyTypeText>(&one_part.part_body)) {
            log_info("{}TEXT (subtype: {})", indent_str, text_part->media_subtype);
        } else if (auto* basic_part = std::get_if<BodyTypeBasic>(&one_part.part_body)) {
            log_info("{}BASIC (type: {}, subtype: {})", indent_str, basic_part->media_type,
                     basic_part->media_subtype);
        } else if (auto* msg_part = std::get_if<BodyTypeMsg>(&one_part.part_body)) {
            log_info("{}MESSAGE", indent_str);
        }
    } else {
        auto& multi_part = *std::get<std::unique_ptr<BodyTypeMPart>>(body);
        log_info("{}MULTI (subtype: {})", indent_str, multi_part.media_subtype);

        for (auto& subpart : multi_part.body_ptrs) {
            log_info("{}traverse bodypart at level {}", indent_str, indent_width);
            traverse_body(subpart, indent_width + 4);
        }

        log_info("{}END MULTI", indent_str);
    }
}

expected<void> capture_attachments_metadata(const imap_parser::wip::Body& body,
                                            emailkit::types::MailboxEmail& mail) {
    // The algorithm is going to be the following:
    // If there is alternative section with text/html than all 0-level parts inclyding TEXT one
    // capturd as attachments. If there is no, then zero level TEXT parts is considered as not an
    // attachment and thus ignored.
    // Related RFC and materials:
    // https://stackoverflow.com/questions/64687378/how-many-text-plain-and-text-html-parts-can-an-email-have
    // https://www.w3.org/Protocols/rfc1341/7_2_Multipart.html

    auto capture_attachment_name = [](const auto& field_params, std::string& name_lvalue) -> void {
        for (auto& [p, v] : field_params) {
            // never actually seen NAME without quotes but stil does not look
            // harmful either.
            if (p == "\"NAME\"" || p == "NAME") {
                name_lvalue = emailkit::utils::strip(v, '\"');
            }
        }
    };

    if (std::holds_alternative<std::unique_ptr<BodyType1Part>>(body)) {
        // TODO: theoretically, email can have the only one part and this part can be non-text so
        // this can be considered as attachments. Say, we send just one file in wierd way.
        auto& one_part = *std::get<std::unique_ptr<BodyType1Part>>(body);
        if (auto* basic_part = std::get_if<BodyTypeBasic>(&one_part.part_body)) {
            log_warning("this must be rare: the only single part is basic");
            mail.attachments.emplace_back(basic_part->media_type, basic_part->media_subtype);
            capture_attachment_name(basic_part->body_fields.params, mail.attachments.back().name);
        }
    } else {
        assert(std::holds_alternative<std::unique_ptr<BodyTypeMPart>>(body));
        auto& multi_part = *std::get<std::unique_ptr<BodyTypeMPart>>(body);
        if (multi_part.media_subtype == "MIXED") {
            log_debug("skipping non-mixed multipart");
            for (auto& subpart : multi_part.body_ptrs) {
                // Take only first-level BASIC and TEXT parts
                if (std::holds_alternative<std::unique_ptr<BodyType1Part>>(subpart)) {
                    auto& one_part = *std::get<std::unique_ptr<BodyType1Part>>(subpart);
                    if (auto* text_part = std::get_if<BodyTypeText>(&one_part.part_body)) {
                        // TODO: use a constant.
                        mail.attachments.emplace_back("TEXT", text_part->media_subtype);
                    } else if (auto* basic_part = std::get_if<BodyTypeBasic>(&one_part.part_body)) {
                        mail.attachments.emplace_back(basic_part->media_type,
                                                      basic_part->media_subtype);
                        capture_attachment_name(basic_part->body_fields.params,
                                                mail.attachments.back().name);
                        mail.attachments.back().octets = basic_part->body_fields.octets;
                    } else if (auto* msg_part = std::get_if<BodyTypeMsg>(&one_part.part_body)) {
                        log_warning("skipping MESSAGE part type on first second level");
                    }
                } else {
                    log_debug("skipping nested multipart");
                    continue;
                }
            }
        }
    }

    return {};
}

}  // namespace

namespace imap_commands {
expected<std::string> encode_cmd(const fetch_t& cmd) {
    const auto encoded_items = std::visit(
        overload{
            [&](all_t x) -> std::string { return "all"; },
            [&](fast_t x) -> std::string { return "fast"; },
            [&](full_t x) -> std::string { return "full"; },
            [&](const fetch_items_vec_t& x) -> std::string {
                std::vector<std::string> items;
                for (auto& i : x) {
                    auto item_encoded = std::visit(
                        overload{
                            [&](fetch_items::body_t x) -> std::string { return "body"; },
                            // TODO: not supported yet, encoding is complicated here.
                            // [&](fetch_items::body_part_t x) -> std::string {
                            //     return fmt::format("body[{}]", x.section_spec);
                            // },
                            // [&](fetch_items::body_peek_t x) -> std::string {
                            //     return fmt::format("[{}]", x.section_spec);
                            // },
                            [&](fetch_items::body_structure_t x) -> std::string {
                                return "bodystructure";
                            },
                            [&](fetch_items::envelope_t x) -> std::string { return "envelope"; },
                            [&](fetch_items::flags_t x) -> std::string { return "flags"; },
                            [&](fetch_items::internal_date_t x) -> std::string {
                                return "internaldate";
                            },
                            [&](fetch_items::rfc822_t x) -> std::string { return "rfc822"; },
                            [&](fetch_items::rfc822_header_t x) -> std::string {
                                return "rfc822.header";
                            },
                            [&](fetch_items::rfc822_size_t x) -> std::string {
                                return "rfc822.size";
                            },
                            [&](fetch_items::rfc822_text_t x) -> std::string {
                                return "rfc822.text";
                            },
                            [&](fetch_items::uid_t x) -> std::string { return "uid"; },
                            [&](fetch_items_raw_string_t x) -> std::string {
                                return std::move(x);
                            }},
                        i);
                    items.emplace_back(std::move(item_encoded));
                }
                return fmt::format("({})", fmt::join(items, " "));
            },
        },
        cmd.items);

    auto encoded_sequence_set =
        std::visit(overload{[](const std::string& x) -> std::string { return x; },
                            [](const fetch_sequence_spec& x) -> std::string {
                                return fmt::format("{}:{}", x.from, x.to);
                            }},
                   cmd.sequence_set);

    return fmt::format("fetch {} {}", encoded_sequence_set, encoded_items);
}

}  // namespace imap_commands

namespace {
class imap_client_impl_t : public imap_client_t, public EnableUseThis<imap_client_impl_t> {
   public:
    explicit imap_client_impl_t(asio::io_context& ctx) : m_ctx(ctx) {}

    bool initialize(std::string tag_pattern) {
        m_imap_socket = make_imap_socket(m_ctx);
        if (!m_imap_socket) {
            log_info("make_imap_socket failed");
            return false;
        }
        m_imap_socket->set_option(imap_socket_opts::dump_stream_to_file{});
        m_tag_pattern = tag_pattern;
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
                    log_debug("NO/BAD received during attempt to authinicate");
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
            [this, command, tag, cb = std::move(cb)](std::error_code ec) mutable {
                if (ec) {
                    log_error("failed sending namespace command: {}", ec);
                    cb(ec, {});
                    return;
                }

                log_info("sent command {}, receiving response ...", command);

                m_imap_socket->async_receive_response(
                    tag, [this, tag, cb = std::move(cb)](std::error_code ec,
                                                         std::string response) mutable {
                        if (ec) {
                            cb(ec, std::move(response));
                            return;
                        }

                        // log_debug("{} command finished, text is: '{}'", tag,
                        //           utils::escape_ctrl(response));
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
                            .flags = parsed_line.mailbox_list_flags,
                            .hierarchy_delimiter = parsed_line.hierarchy_delimiter});
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
            fmt::format("select \"{}\"", cmd.mailbox_name),
            [cb = std::move(cb)](std::error_code ec, imap_response_t imap_resp) mutable {
                if (ec) {
                    log_error("async_execute_simple_command failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                // TODO: check if this can be improved. Questions are: is it only first line that
                // can be NO response?
                // TODO: add test.
                // TODO: check other commands for this mistake.
                if (!imap_resp.lines.empty() &&
                    (imap_resp.lines[0].is_no_response() || imap_resp.lines[0].is_bad_response())) {
                    log_error("got bad/no response for select");
                    cb(make_error_code(std::errc::io_error), {});
                    return;
                }

                // Parsing response
                // TODO: write a test for this
                // 'A4 NO [NONEXISTENT] Unknown Mailbox: [Gmail] (now in authenticated state)
                // (Failure)\r\n'
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
        auto encoded_cmd_or_err = encode_cmd(cmd);
        if (!encoded_cmd_or_err) {
            log_error("failed encoding fetch command: {}", encoded_cmd_or_err.error());
            cb(encoded_cmd_or_err.error(), {});
            return;
        }
        auto& encoded_cmd = *encoded_cmd_or_err;

        async_execute_raw_command(encoded_cmd, [cb = std::move(cb)](std::error_code ec,
                                                                    std::string imap_resp) mutable {
            if (ec) {
                log_error("async_execute_simple_command failed: {}", ec);
                cb(ec, {});
                return;
            }

            // TODO: add check for NO/BAD.

            log_info("parsing fetch response of size {}Kb", imap_resp.size() / 1024);

            auto parse_start = std::chrono::steady_clock::now();
            auto message_data_records_or_err = imap_parser::parse_message_data_records(imap_resp);
            if (!message_data_records_or_err) {
                log_error("failed parsing message data: {}", message_data_records_or_err.error());
                cb(message_data_records_or_err.error(), {});
                return;
            }
            auto parse_took = std::chrono::steady_clock::now() - parse_start;

            log_info("parsing successful, time taken: {}ms", parse_took / 1.0ms);

            cb({}, types::fetch_response_t{.message_data_items =
                                               std::move(*message_data_records_or_err)});
        });
    }

    ////////////////////////////////////////////////////////////////////////////////////////

    void async_list_mailboxes(async_callback<ListMailboxesResult> cb) override {
        async_execute_command(
            imap_commands::list_t{.reference_name = "", .mailbox_name = "*"},
            use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                       types::list_response_t response, auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async list command failed");
                // TODO: this is not final. Just a stupid remap for now.
                // Here is a something looking like RFC for flags.
                // It should be checked if this Standard/Extension is possible to query via
                // CAPABILITIES command but it looks primising. If this is true, we can decode this
                // names into enumeration. And know what is JUNK folder in RFC standard way:
                // IMAP LIST extension for special-use mailboxes draft-ietf-morg-list-specialuse-02
                // https://datatracker.ietf.org/doc/id/draft-ietf-morg-list-specialuse-02.html

                cb({}, ListMailboxesResult{.raw_response = std::move(response)});
            }));
    }

    void async_select_mailbox(std::string inbox_name,
                              async_callback<SelectMailboxResult> cb) override {
        async_execute_command(
            imap_commands::select_t{.mailbox_name = inbox_name},
            use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                       types::select_response_t response, auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async select command failed");

                log_debug("selected INBOX folder (exists: {}, recents: {})", response.exists,
                          response.recents);

                cb({}, {.raw_response = std::move(response)});
            }));
    }

    void async_list_items(int from,
                          std::optional<int> to,
                          async_callback<std::vector<emailkit::types::MailboxEmail>> cb) override {
        async_execute_command(
            imap_commands::fetch_t{
                .sequence_set = imap_commands::raw_fetch_sequence_spec{fmt::format(
                    "{}:{}", from, to.has_value() ? std::to_string(*to) : "*")},
                .items =
                    imap_commands::fetch_items_vec_t{
                        imap_commands::fetch_items::uid_t{},
                        imap_commands::fetch_items::body_structure_t{},
                        imap_commands::fetch_items::rfc822_header_t{}}},
            use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                       types::fetch_response_t response, auto cb) {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async fetch command failed");

                std::vector<emailkit::types::MailboxEmail> result;

                for (auto& [message_number, static_attributes] : response.message_data_items) {
                    emailkit::types::MailboxEmail current_email;

                    if (static_attributes.size() > 3) {
                        log_warning(
                            "unexpected static attributes alongside of bodystructure, will be "
                            "ignored ({})",
                            static_attributes.size());
                    }

                    for (auto& sattr : static_attributes) {
                        if (std::holds_alternative<imap_parser::wip::Body>(sattr)) {
                            auto& as_body = std::get<imap_parser::wip::Body>(sattr);
                            if (!capture_attachments_metadata(as_body, current_email)) {
                                log_error("failed capturing attachements");
                                // TODO: use some blank/dumyy emails instead or leave partially
                                // parsed emails so user can see details.
                                continue;
                            }
                        } else if (std::holds_alternative<imap_parser::msg_attr_uid_t>(sattr)) {
                            auto& as_uid = std::get<imap_parser::msg_attr_uid_t>(sattr);
                            current_email.message_uid = as_uid.value;
                        } else if (std::holds_alternative<imap_parser::MsgAttrRFC822>(sattr)) {
                            auto& as_rfc822 = std::get<imap_parser::MsgAttrRFC822>(sattr);
                            auto parser =
                                imap_parser::rfc822::parse_rfc882_message(as_rfc822.msg_data);
                            if (!capture_headers(parser, current_email)) {
                                log_error("invalid email, skipping");
                                // TODO: use some blank/dummy emails instead
                                continue;
                            }

                        } else {
                            log_warning("ignoring unexpected non-bodystructure static attribute");
                            continue;
                        }
                    }

                    result.emplace_back(std::move(current_email));
                }
                cb({}, std::move(result));
            }));
    }

    ////////////////////////////////////////////////////////////////////////////////////////

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

            // check if line is tagged response (TODO: do it right, with parser or make sure
            // it is correct according to the grammar)
            bool stop_reading = false;
            if (line.rfind(tag, 0) == 0) {
                log_debug("got tagged reply in line '{}', stop reading..", line);
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
                log_debug("got tag, stopping..");
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

    std::string new_command_id() {
        return fmt::format(fmt::runtime(m_tag_pattern), m_command_counter++);
    }
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
    std::string m_tag_pattern;
};  // namespace

}  // namespace

std::shared_ptr<imap_client_t> make_test_imap_client(asio::io_context& ctx) {
    auto client = std::make_shared<imap_client_impl_t>(ctx);
    if (!client->initialize("A{}")) {
        return nullptr;
    }
    return client;
}

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx) {
    auto client = std::make_shared<imap_client_impl_t>(ctx);
    if (!client->initialize("A{}")) {
        return nullptr;
    }
    return client;
}

}  // namespace emailkit::imap_client
