#pragma once
#include <string_view>

namespace emailkit::imap_parser {

void parse_mailbox_data(std::string_view input);

void parse_flags_list(std::string_view input);

}  // namespace emailkit::imap_parser