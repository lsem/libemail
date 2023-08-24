#pragma once
#include <emailkit/global.hpp>
#include "imap_parser_types.hpp"

#include <string_view>

namespace emailkit::imap_parser {

expected<list_response_t> parse_list_response_line(std::string_view input);
expected<std::vector<mailbox_data_t>> parse_mailbox_data_records(std::string_view input_text);

}  // namespace emailkit::imap_parser