#pragma once

#include <string>
#include <variant>
#include <vector>

#include <emailkit/global.hpp>

namespace emailkit::imap_client::types {

struct list_response_entry_t {
    std::string mailbox_raw;
    std::vector<std::string> inbox_path;  // TODO: why it is called inbox path but not mailbox path?
                                          // we should have called it mailbox_path_parts
    std::vector<std::string> flags;
};

struct list_response_t {
    std::vector<list_response_entry_t> inbox_list;
};

struct select_response_t {
    // ..
};

enum class imap_errors {
    // no 0
    imap_bad = 1,
    imap_no,
};

std::error_code make_error_code(imap_errors e);

}  // namespace emailkit::imap_client::types

namespace std {
template <>
struct is_error_code_enum<emailkit::imap_client::types::imap_errors> : true_type {};
}  // namespace std

DEFINE_FMT_FORMATTER(emailkit::imap_client::types::list_response_entry_t,
                     "{{inbox_path: {}, flags: {}}}",
                     arg.inbox_path,
                     arg.flags);

DEFINE_FMT_FORMATTER(emailkit::imap_client::types::list_response_t,
                     "list_response_t({})",
                     arg.inbox_list);
