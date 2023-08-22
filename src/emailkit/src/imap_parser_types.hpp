#pragma once

#include <string>
#include <variant>
#include <vector>
#include <optional>

namespace emailkit::imap_parser {

struct list_response_t {
    std::string mailbox;
    std::vector<std::string> mailbox_list_flags;
    std::string hierarchy_delimiter;
};

enum class read_write_mode_t {
    na,  // not available.
    read_write,
    read_only,
    try_create,
};

struct select_response_data_t {
    int recents = 0;
    int exists = 0;
    int uid_validity = 0;
    std::optional<int> unseen;
    int uid_next = 0;
    std::vector<std::string> flags;
    std::vector<std::string> permanent_flags;
    read_write_mode_t read_write_mode = read_write_mode_t::na;
};

namespace utils {
std::vector<std::string> decode_mailbox_path_from_list_response(const list_response_t& r);
}

std::string to_json(const list_response_t&);

}  // namespace emailkit::imap_parser
