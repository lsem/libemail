#include "imap_parser__rfc822.hpp"
#include <gmime/gmime.h>
#include "imap_parser.hpp"
#include "utils.hpp"

#include <scope_guard/scope_guard.hpp>

namespace emailkit::imap_parser::rfc822 {

struct RFC822ParserState {
    GMimeStream* stream = nullptr;
    GMimeParser* parser = nullptr;
    GMimeMessage* message = nullptr;

    ~RFC822ParserState() {
        if (stream) {
            g_object_unref(stream);
        }
        if (parser) {
            g_object_unref(parser);
        }
        if (message) {
            g_object_unref(message);
        }
    }
};
namespace {
emailkit::types::EmailAddressVec gmime_internet_address_to_address_vec(InternetAddressList* list) {
    emailkit::types::EmailAddressVec result;
    const int list_size = internet_address_list_length(list);
    log_debug("internet address list length: {}", list_size);

    for (int i = 0; i < list_size; ++i) {
        emailkit::types::EmailAddress current_address;

        auto* addr = internet_address_list_get_address(list, i);
        if (!addr) {
            log_warning("{} is null address", i);
            continue;
        }

        if (INTERNET_ADDRESS_IS_MAILBOX(addr)) {
            log_debug("is a mailbox");
            InternetAddressMailbox* as_mailbox = reinterpret_cast<InternetAddressMailbox*>(addr);
            const char* address_part = internet_address_mailbox_get_addr(as_mailbox);

            // Seems like we don't have libIDN and I doubt we really need it. The address_part is
            // already what we need user@example.com.
            //            const char* idn_part = internet_address_mailbox_get_idn_addr(as_mailbox);

            log_debug("parsed mailbox: {}", address_part);

            result.emplace_back(address_part);
        } else if (INTERNET_ADDRESS_IS_GROUP(addr)) {
            log_debug("is a group");
            InternetAddressGroup* as_group = reinterpret_cast<InternetAddressGroup*>(addr);
        } else {
            log_warning("something else, skip");
        }
    }

    return result;
}

}  // namespace

expected<void> initialize() {
    ::g_mime_init();
    log_debug("GMIME init called");
    return {};
}
expected<void> finalize() {
    ::g_mime_shutdown();
    log_debug("GMIME shutdown called");
    return {};
}

namespace {

#if 0
static void process_part(GMimeObject* parent, GMimeObject* part, void* user_data) {
    static int counter = 0;
    ++counter;

    /* find out wvhat class 'part' is... */
    if (GMIME_IS_MESSAGE_PART(part)) {
        /* message/rfc822 or message/news */
        GMimeMessage* message;

        log_debug("message/rfc822 or message/news");

        /* g_mime_message_foreach() won't descend into
           child message parts, so if we want to count any
           subparts of this child message, we'll have to call
           g_mime_message_foreach() again here. */

        message = g_mime_message_part_get_message((GMimeMessagePart*)part);
        g_mime_message_foreach(message, process_part, user_data);
    } else if (GMIME_IS_MESSAGE_PARTIAL(part)) {
        /* message/partial */

        log_debug("message/partial");

        /* this is an incomplete message part, probably a
           large message that the sender has broken into
           smaller parts and is sending us bit by bit. we
           could save some info about it so that we could
           piece this back together again once we get all the
           parts? */
    } else if (GMIME_IS_MULTIPART(part)) {
        /* multipart/mixed, multipart/alternative,
         * multipart/related, multipart/signed,
         * multipart/encrypted, etc... */

        log_debug("multipart/mixed");

        /* we'll get to finding out if this is a
         * signed/encrypted multipart later... */
    } else if (GMIME_IS_PART(part)) {
        /* a normal leaf part, could be text/plain or
         * image/jpeg etc */
        log_empty_line();
        log_info("processing part ..");

        GMimeHeaderList* header_list = g_mime_object_get_header_list(part);
        if (header_list) {
            auto rfc822_headers_or_err = decode_headers_from_part(part);
            if (rfc822_headers_or_err) {
                const rfc822_headers_t& rfc822_headers = *rfc822_headers_or_err;
                for (auto& [k, v] : rfc822_headers) {
                    log_info("'{}': '{}'", k, v);
                }

            } else {
                log_error("no headers in rfc822 message: {}", rfc822_headers_or_err.error());
            }

            // static const std::set<std::string> envelope_headers = {
            //     envelope_fields::date,      envelope_fields::subject, envelope_fields::subject,
            //     envelope_fields::from,      envelope_fields::sender,  envelope_fields::reply_to,
            //     envelope_fields::to,        envelope_fields::cc,      envelope_fields::bcc,
            //     envelope_fields::message_id};
        }

        auto content_type_or_err = get_part_content_type(part);
        if (!content_type_or_err) {
            log_error("failed parsing content type: {}", content_type_or_err.error());
            return;
        }
        const auto& content_type = *content_type_or_err;

        if (content_type.type == "text" && content_type.media_subtype == "html") {
            log_info("found text/html, here are params:");
            for (auto& [p, v] : content_type.params) {
                log_info("  {}: {}", p, v);
            }

            auto text_html_or_err = decode_html_content_from_part(part);
            if (!text_html_or_err) {
                log_error("failed decoding html from part: {}", text_html_or_err.error());
                return;
            }
            const auto& text_html = *text_html_or_err;

            std::string file_name = fmt::format("part_content_html_{}.html", counter);
            std::ofstream file_stream(file_name, std::ios_base::out);
            if (file_stream) {
                file_stream << text_html;

                if (file_stream.good()) {
                    log_info("decoded html for part, stored in file: {}", file_name);
                } else {
                    log_error("failed writing file for part.size was: {}", text_html.size());
                }
            }

        } else if (content_type.type == "text" && content_type.media_subtype == "plain") {
            // TODO:
            log_info("skipping text/plain");
            return;
        } else if (content_type.type == "image") {
            auto image_data_or_err = decode_image_content_from_part(part);
            if (!image_data_or_err) {
                log_error("failed decoding image data for part: {}", image_data_or_err.error());
                return;
            }
            auto& image_data = *image_data_or_err;

            std::string file_name = fmt::format("part_content_html_{}.png", counter);
            std::ofstream file_stream(file_name, std::ios_base::out | std::ios_base::binary);
            if (file_stream) {
                file_stream.write(image_data.data.data(), image_data.data.size());

                if (file_stream.good()) {
                    log_info("decoded png part, stored in file: {}", file_name);
                } else {
                    log_error("failed writing file png data for part.size was: {}",
                              image_data.data.size());
                }
            }

        } else {
            log_info("skipping unknown yet part: {}/{}", content_type.type,
                     content_type.media_subtype);
        }

    } else {
        g_assert_not_reached();
    }
}

expected<void> decode_mesage_parts(GMimeMessage* message) {
    // TODO: why message but not part?
    g_mime_message_foreach(message, &process_part, nullptr);
    return {};
}
#endif

expected<rfc822_headers_t> decode_headers_from_part(GMimeObject* part) {
    rfc822_headers_t rfc822_headers;

    GMimeHeaderList* header_list = g_mime_object_get_header_list(part);
    if (!header_list) {
        log_error("no headers in part");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    const int headers_num = g_mime_header_list_get_count(header_list);
    for (int i = 0; i < headers_num; ++i) {
        auto header = g_mime_header_list_get_header_at(header_list, i);
        if (!header) {
            log_warning("no header at {}", i);
            continue;
        }
        auto header_name = g_mime_header_get_name(header);
        if (!header_name) {
            log_warning("header at {} does not have a name", i);
            continue;
        }

        auto header_value = g_mime_header_get_value(header);
        if (!header_value) {
            log_warning("header at {} does not have a value", i);
            continue;
        }

        rfc822_headers.emplace_back(header_name, header_value);
    }

    return std::move(rfc822_headers);
}

}  // namespace

expected<void> parse_rfc822_message(std::string_view message_data) {
    // GMimeStream* stream =
    //     g_mime_stream_mem_new_with_buffer(message_data.data(), message_data.size());
    // if (!stream) {
    //     log_error("failed creating stream");
    //     return unexpected(make_error_code(parser_errc::parser_fail_l2));
    // }
    // GMimeParser* parser = g_mime_parser_new_with_stream(stream);
    // if (!parser) {
    //     log_error("failed creating parser from stream");
    //     return unexpected(make_error_code(parser_errc::parser_fail_l2));
    // }
    // log_debug("parser created");

    // GMimeMessage* message = g_mime_parser_construct_message(parser, nullptr);
    // if (!message) {
    //     log_error("failed constructing message from parser");
    //     return unexpected(make_error_code(parser_errc::parser_fail_l2));
    // }

    // log_debug("message parsed, message id: {}", message->message_id);

    // // now we are going to get the following:
    // //  1. header
    // //  2. bodystructure
    // //  3. parts

    // auto rfc822_headers_or_err = decode_headers_from_part((GMimeObject*)message);
    // if (!rfc822_headers_or_err) {
    //     log_error("no headers in rfc822 message: {}", rfc822_headers_or_err.error());
    //     return unexpected(rfc822_headers_or_err.error());
    // }
    // const rfc822_headers_t& rfc822_headers = *rfc822_headers_or_err;

    // static const std::set<std::string> envelope_headers = {
    //     envelope_fields::date,      envelope_fields::subject, envelope_fields::subject,
    //     envelope_fields::from,      envelope_fields::sender,  envelope_fields::reply_to,
    //     envelope_fields::to,        envelope_fields::cc,      envelope_fields::bcc,
    //     envelope_fields::message_id};
    // for (auto& [k, v] : rfc822_headers) {
    //     if (envelope_headers.count(k) > 0) {
    //         log_info("'{}': '{}'", k, v);
    //     }
    // }

    // auto nothing_or_err = decode_mesage_parts(message);
    // if (!nothing_or_err) {
    //     log_error("failed decoding message parts: {}", nothing_or_err.error());
    //     return unexpected(nothing_or_err.error());
    // }

    return {};
}
expected<imap_parser::rfc822_headers_t> parse_headers_from_rfc822_message(
    std::string_view message_data) {
    GMimeStream* stream =
        g_mime_stream_mem_new_with_buffer(message_data.data(), message_data.size());
    if (!stream) {
        log_error("failed creating stream");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    auto stream_guard = sg::make_scope_guard([stream] { g_object_unref(stream); });

    GMimeParser* parser = g_mime_parser_new_with_stream(stream);
    if (!parser) {
        log_error("failed creating parser from stream");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }
    log_debug("parser created");

    auto parser_guard = sg::make_scope_guard([parser] { g_object_unref(parser); });

    GMimeMessage* message = g_mime_parser_construct_message(parser, nullptr);
    if (!message) {
        log_error("failed constructing message from parser");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    auto message_guard = sg::make_scope_guard([message] { g_object_unref(message); });

    log_debug("message parsed, message id: {}", message->message_id);

    auto rfc822_headers_or_err = decode_headers_from_part((GMimeObject*)message);
    if (!rfc822_headers_or_err) {
        log_error("no headers in rfc822 message: {}", rfc822_headers_or_err.error());
        return unexpected(rfc822_headers_or_err.error());
    }
    const rfc822_headers_t& rfc822_headers = *rfc822_headers_or_err;

#ifndef _NDEBUG
    for (auto& [header, value] : rfc822_headers) {
        log_debug("parse_headers_from_rfc822_message: {}: {}", header, value);
    }
#endif

    return std::move(rfc822_headers);
}

RFC822ParserStateHandle parse_rfc882_message(std::string_view message_data) {
    auto result = std::make_shared<RFC822ParserState>();
    result->stream = g_mime_stream_mem_new_with_buffer(message_data.data(), message_data.size());
    if (!result->stream) {
        log_error("failed creating stream");
        return nullptr;
    }

    result->parser = g_mime_parser_new_with_stream(result->stream);
    if (!result->parser) {
        log_error("failed creating parser from stream");
        return nullptr;
    }
    log_debug("parser created");

    result->message = g_mime_parser_construct_message(result->parser, nullptr);
    if (!result->message) {
        log_error("failed constructing message from parser");
        return nullptr;
    }

    return result;
}

expected<std::map<std::string, std::string>> get_headers(RFC822ParserStateHandle state) {
    std::map<std::string, std::string> result;

    GMimeHeaderList* list =
        g_mime_object_get_header_list(reinterpret_cast<GMimeObject*>(state->message));
    if (!list) {
        log_warning("no headers in a message");
        return unexpected(make_error_code(std::errc::io_error));
    }

    auto list_size = g_mime_header_list_get_count(list);

    for (int i = 0; i < list_size; ++i) {
        auto header = g_mime_header_list_get_header_at(list, i);
        if (!header) {
            log_warning("null returned for header {}", i);
            continue;
        }
        result.emplace(g_mime_header_get_name(header), g_mime_header_get_value(header));
    }

    return result;
}

emailkit::types::EmailDate to_emaildate(GDateTime* dt) {
    return emailkit::types::EmailDate{.year = g_date_time_get_year(dt),
                                      .month = g_date_time_get_month(dt),
                                      .day = g_date_time_get_day_of_month(dt),
                                      .hours = g_date_time_get_hour(dt),
                                      .minutes = g_date_time_get_minute(dt),
                                      .seconds = g_date_time_get_second(dt)

    };
}

std::optional<emailkit::types::EmailDate> get_date(RFC822ParserStateHandle state) {
    GDateTime* date = g_mime_message_get_date(state->message);
    if (!date) {
        log_error("no Date");
        return {};
    }

    return to_emaildate(g_date_time_to_utc(date));
}

std::optional<std::string> get_subject(RFC822ParserStateHandle state) {
    const char* msg = g_mime_message_get_subject(state->message);
    if (!msg) {
        return std::nullopt;
    }

    return msg;
}

std::optional<emailkit::types::EmailAddressVec> get_from_address(RFC822ParserStateHandle state) {
    InternetAddressList* addr_list = g_mime_message_get_from(state->message);
    if (!addr_list) {
        log_warning("no FROM list");
        return {};
    }
    return gmime_internet_address_to_address_vec(addr_list);
}

std::optional<emailkit::types::EmailAddressVec> get_to_address(RFC822ParserStateHandle state) {
    InternetAddressList* addr_list = g_mime_message_get_to(state->message);
    if (!addr_list) {
        log_warning("no TO list");
        return {};
    }
    return gmime_internet_address_to_address_vec(addr_list);
}

std::optional<emailkit::types::EmailAddressVec> get_cc_address(RFC822ParserStateHandle state) {
    InternetAddressList* addr_list = g_mime_message_get_cc(state->message);
    if (!addr_list) {
        log_warning("no CC list");
        return {};
    }
    return gmime_internet_address_to_address_vec(addr_list);
}
std::optional<emailkit::types::EmailAddressVec> get_bcc_address(RFC822ParserStateHandle state) {
    InternetAddressList* addr_list = g_mime_message_get_bcc(state->message);
    if (!addr_list) {
        log_warning("no BCC list");
        return {};
    }
    return gmime_internet_address_to_address_vec(addr_list);
}

std::optional<emailkit::types::EmailAddressVec> get_sender_address(RFC822ParserStateHandle state) {
    InternetAddressList* addr_list = g_mime_message_get_sender(state->message);
    if (!addr_list) {
        log_warning("no SENDER list");
        return {};
    }
    return gmime_internet_address_to_address_vec(addr_list);
}
std::optional<emailkit::types::EmailAddressVec> get_reply_to_address(
    RFC822ParserStateHandle state) {
    InternetAddressList* addr_list = g_mime_message_get_reply_to(state->message);
    if (!addr_list) {
        log_warning("no REPLY_TO list");
        return {};
    }
    return gmime_internet_address_to_address_vec(addr_list);
}

std::optional<emailkit::types::MessageID> get_message_id(RFC822ParserStateHandle state) {
    // GMimeHeaderList *g_mime_object_get_header_list (GMimeObject *object);
    //     const char *g_mime_header_get_name (GMimeHeader *header);
    // const char *g_mime_header_get_raw_name (GMimeHeader *header);
    // const char *g_mime_header_get_value (GMimeHeader *header);

    const char* msg_id = g_mime_message_get_message_id(state->message);
    if (!msg_id) {
        log_warning("no MessageID");
        return {};
    }
    return msg_id;
}

std::optional<emailkit::types::MessageID> get_in_reply_to(RFC822ParserStateHandle state) {
    GMimeHeaderList* list =
        g_mime_object_get_header_list(reinterpret_cast<GMimeObject*>(state->message));
    if (!list) {
        log_warning("no headers in a message");
        return {};
    }

    auto list_size = g_mime_header_list_get_count(list);

    for (int i = 0; i < list_size; ++i) {
        auto header = g_mime_header_list_get_header_at(list, i);
        if (!header) {
            log_warning("null returned for header {}", i);
            continue;
        }
        auto name = g_mime_header_get_name(header);
        if (std::strcmp(name, "In-Reply-To") == 0) {
            return g_mime_header_get_value(header);
        }
    }

    return {};
}

expected<void> collect_headers(RFC822ParserStateHandle state, emailkit::types::MailboxEmail& mail) {
    {
        InternetAddressList* addr_list = g_mime_message_get_from(state->message);
        if (!addr_list) {
            log_error("no FROM list");
            return unexpected(make_error_code(std::errc::io_error));
        }
        mail.from = gmime_internet_address_to_address_vec(addr_list);
    }
    {
        InternetAddressList* addr_list = g_mime_message_get_to(state->message);
        if (!addr_list) {
            log_error("no TO list");
            return unexpected(make_error_code(std::errc::io_error));
        }
        mail.to = gmime_internet_address_to_address_vec(addr_list);
    }
    {
        InternetAddressList* addr_list = g_mime_message_get_cc(state->message);
        if (!addr_list) {
            log_error("no CC list");
            return unexpected(make_error_code(std::errc::io_error));
        }
        mail.cc = gmime_internet_address_to_address_vec(addr_list);
    }
    {
        InternetAddressList* addr_list = g_mime_message_get_bcc(state->message);
        if (!addr_list) {
            log_error("no BCC list");
            return unexpected(make_error_code(std::errc::io_error));
        }
        mail.bcc = gmime_internet_address_to_address_vec(addr_list);
    }

    {
        InternetAddressList* addr_list = g_mime_message_get_sender(state->message);
        if (!addr_list) {
            log_warning("no SENDER list");
            return unexpected(make_error_code(std::errc::io_error));
        }
        mail.sender = gmime_internet_address_to_address_vec(addr_list);
    }

    {
        InternetAddressList* addr_list = g_mime_message_get_reply_to(state->message);
        if (!addr_list) {
            log_warning("no REPLY_TO list");
            return unexpected(make_error_code(std::errc::io_error));
        }
        mail.reply_to = gmime_internet_address_to_address_vec(addr_list);
    }

    {
        const char* subject = g_mime_message_get_subject(state->message);
        if (!subject) {
            log_error("no MessageID");
            return unexpected(make_error_code(std::errc::io_error));
        }
        mail.subject = subject;
    }

    // TODO: DATE!
    GDateTime* date = g_mime_message_get_date(state->message);
    if (!date) {
        log_error("no Date");
        return unexpected(make_error_code(std::errc::io_error));
    }

    //
    // Non mandatory headers.
    //

    {
        const char* msg_id = g_mime_message_get_message_id(state->message);
        if (!msg_id) {
            log_warning("no MessageID");
            // NOTE, this is optional. The server may not use MessageID as far as I can tell.
        }
        mail.message_id = msg_id;
    }

    {
        GMimeHeaderList* list =
            g_mime_object_get_header_list(reinterpret_cast<GMimeObject*>(state->message));
        if (!list) {
            log_warning("no headers in a message");
            return {};
        }
        auto list_size = g_mime_header_list_get_count(list);

        for (int i = 0; i < list_size; ++i) {
            auto header = g_mime_header_list_get_header_at(list, i);
            if (!header) {
                log_warning("null returned for header {}", i);
                continue;
            }
            auto name = g_mime_header_get_name(header);

            if (std::strcmp(name, "In-Reply-To") == 0) {
                mail.in_reply_to = g_mime_header_get_value(header);
            }
        }
    }

    return {};
}

}  // namespace emailkit::imap_parser::rfc822
