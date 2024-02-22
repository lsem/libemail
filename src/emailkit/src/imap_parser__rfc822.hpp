#pragma once
#include <emailkit/global.hpp>
#include "imap_parser_types.hpp"
#include "types.hpp"

namespace emailkit::imap_parser::rfc822 {

expected<void> initialize();
expected<void> finalize();

/////////////////////////////////////////////////////////////////////////////

expected<void> parse_rfc822_message(std::string_view message_data);
expected<imap_parser::rfc822_headers_t> parse_headers_from_rfc822_message(
    std::string_view message_data);

//////////////////////////////////////////////////////////////////////////////

struct RFC822ParserState;
using RFC822ParserStateHandle = std::shared_ptr<RFC822ParserState>;

RFC822ParserStateHandle parse_rfc882_message(std::string_view message_data);
std::optional<std::string> get_subject(RFC822ParserStateHandle state);
std::optional<emailkit::types::EmailAddressVec> get_from_address(RFC822ParserStateHandle);
std::optional<emailkit::types::EmailAddressVec> get_to_address(RFC822ParserStateHandle);
std::optional<emailkit::types::EmailAddressVec> get_cc_address(RFC822ParserStateHandle);
std::optional<emailkit::types::EmailAddressVec> get_bcc_address(RFC822ParserStateHandle);
std::optional<emailkit::types::EmailAddressVec> get_sender(RFC822ParserStateHandle);
std::optional<emailkit::types::EmailAddressVec> get_reply_to(RFC822ParserStateHandle);
std::optional<emailkit::types::MessageID> get_message_id(RFC822ParserStateHandle);
std::optional<emailkit::types::MessageID> get_in_reply_to(RFC822ParserStateHandle);

}  // namespace emailkit::imap_parser::rfc822
