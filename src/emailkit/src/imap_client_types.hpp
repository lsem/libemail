#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <emailkit/global.hpp>

#include "imap_parser_types.hpp"

namespace emailkit::imap_client::types {

////////////////////////////////////////////////////////////////////////////////////////////////
// list_response_t
struct list_response_entry_t {
    std::string mailbox_raw;
    std::vector<std::string> inbox_path;  // TODO: why it is called inbox path but not mailbox path?
                                          // we should have called it mailbox_path_parts
    std::vector<std::string> flags;
    std::string hierarchy_delimiter;
};

struct list_response_t {
    std::vector<list_response_entry_t> inbox_list;
};

////////////////////////////////////////////////////////////////////////////////////////////////
// select_response_t
enum class read_write_mode_t {
    na,  // not available.
    read_write,
    read_only,
    try_create,
};

struct select_response_t {
    uint32_t recents{};
    uint32_t exists{};
    uint32_t uid_validity{};
    std::optional<uint32_t> opt_unseen;
    uint32_t uid_next{};
    std::vector<std::string> flags;
    std::vector<std::string> permanent_flags;
    read_write_mode_t read_write_mode = read_write_mode_t::na;
};

////////////////////////////////////////////////////////////////////////////////////////////////
// fetch_response_t
struct fetch_response_t {
    std::vector<imap_parser::MessageData> message_data_items;
};

////////////////////////////////////////////////////////////////////////////////////////////////
// imap_errors
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
