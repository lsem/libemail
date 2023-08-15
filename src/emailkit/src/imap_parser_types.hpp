#pragma once

#include <string>
#include <vector>
#include <variant>

namespace emailkit::imap_parser {

struct list_response_t {
    std::string mailbox;
    std::vector<std::string> mailbox_list_flags;
    std::string hierarchy_delimiter;
};

struct flags_mailbox_data_t {};

struct list_mailbox_data_t {};

struct lsum_mailbox_data_t {};

struct search_mailbox_data_t {};

struct status_mailbox_data_t {};

struct exists_mailbox_data_t {};

struct recent_mailbox_data_t {};

// corresponds to mailbox-data rule in the grammar
using mailbox_data_t = std::variant<flags_mailbox_data_t,
                                    list_mailbox_data_t,
                                    lsum_mailbox_data_t,
                                    search_mailbox_data_t,
                                    status_mailbox_data_t,
                                    exists_mailbox_data_t,
                                    recent_mailbox_data_t>;

namespace utils {
std::vector<std::string> decode_mailbox_path_from_list_response(const list_response_t& r);
}

std::string to_json(const list_response_t&);

}  // namespace emailkit::imap_parser
