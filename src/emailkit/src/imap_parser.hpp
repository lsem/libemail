#pragma once
#include <emailkit/global.hpp>
#include "imap_parser_types.hpp"

#include <string_view>

namespace emailkit::imap_parser {

expected<list_response_t> parse_list_response_line(std::string_view input);

expected<std::vector<mailbox_data_t>> parse_mailbox_data_records(std::string_view input_text);

expected<std::vector<message_data_t>> parse_message_data_records(std::string_view input_text);

expected<void> parse_rfc822_message(std::string_view input_text);

void parse_expression(std::string_view input_text);

///////////////////////////////////////////////////////////////////////////////////////////////
// imap-parser-errors
enum class parser_errc {
    // basic parser failed at grammar level
    parser_fail_l0 = 1,

    // at grammar level parsing is OK, but format is unexpected.
    parser_fail_l1,

    // at grammar level parsing is OK, but downstream parsers provided by 3rd-party parsers failed.
    parser_fail_l2,    
};

std::error_code make_error_code(parser_errc);

}  // namespace emailkit::imap_parser

namespace std {
template <>
struct is_error_code_enum<emailkit::imap_parser::parser_errc> : true_type {};

}  // namespace std