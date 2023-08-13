#pragma once

#include "imap_parser_types.hpp"

#include <string>
#include <vector>

namespace emailkit::imap_parser::utils {

std::vector<std::string> decode_mailbox_path_from_list_response(const list_response_t& r);

}  // namespace emailkit::imap_parser::utils
