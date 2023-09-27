#pragma once
#include <emailkit/global.hpp>

namespace emailkit::imap_parser::rfc822 {

expected<void> parse_rfc822_message(std::string_view message_data);

}