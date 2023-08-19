#pragma once
#include <emailkit/global.hpp>
#include "imap_parser_types.hpp"

#include <string_view>

namespace emailkit::imap_parser {

expected<list_response_t> parse_list_response_line(std::string_view input);
void parse_flags_list(std::string_view input);

expected<select_response_data> parse_select_response(std::string_view input);

}  // namespace emailkit::imap_parser