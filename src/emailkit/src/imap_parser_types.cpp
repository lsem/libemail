#include <emailkit/global.hpp>
#include "imap_parser_types.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace emailkit::imap_parser {

std::string to_json(const list_response_t& x) {
    return fmt::format(R"json(
        {{
            "mailbox": "{}",
            "mailbox_list_flags": {},
            "hierarchy_delimiter": "{}"
        }}
    )json",
                       x.mailbox, x.mailbox_list_flags, x.hierarchy_delimiter);
}

}  // namespace emailkit::imap_parser
