#pragma once
#include <emailkit/global.hpp>
#include "imap_parser_types.hpp"

namespace emailkit::imap_parser::rfc822 {

expected<void> initialize();
expected<void> finalize();

expected<void> parse_rfc822_message(std::string_view message_data);
expected<imap_parser::rfc822_headers_t> parse_headers_from_rfc822_message(
    std::string_view message_data);

}  // namespace emailkit::imap_parser::rfc822
