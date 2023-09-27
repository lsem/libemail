#include "imap_parser.hpp"
#include "imap_parser__rfc822.hpp"
#include <gmime/gmime.h>


namespace emailkit::imap_parser::rfc822 {

expected<void> parse_rfc822_message(std::string_view message_data) {
    GMimeStream* stream = g_mime_stream_mem_new_with_buffer(message_data.data(), message_data.size());
    if (!stream) {
        log_error("failed creating stream");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }
    GMimeParser* parser = g_mime_parser_new_with_stream(stream);
    if (!parser) {
        log_error("failed creating parser from stream");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }
    log_debug("parser created");

    GMimeMessage* message = g_mime_parser_construct_message(parser, nullptr);
    if (!message) {
        log_error("failed constructing message from parser");
        return unexpected(make_error_code(parser_errc::parser_fail_l2));
    }

    log_debug("message parsed, message id: {}", message->message_id);


    return unexpected(make_error_code(std::errc::io_error));
}

}