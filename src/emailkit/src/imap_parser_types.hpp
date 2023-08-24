#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

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

struct flags_mailbox_data_t {
    std::vector<std::string> flags_vec;
};

struct permanent_flags_mailbox_data_t {
    std::vector<std::string> flags_vec;
};
struct uidvalidity_data_t {
    uint32_t value{};
};

// struct list_mailbox_data_t {};
// struct lsum_mailbox_data_t {};
// struct search_mailbox_data_t {};
// struct status_mailbox_data_t {};

struct unseen_resp_text_code_t {
    uint32_t value;
};
struct uidnext_resp_text_code_t {
    uint32_t value{};
};
struct read_write_resp_text_code_t {};
struct read_only_resp_text_code_t {};
struct try_create_resp_text_code_t {};

struct exists_mailbox_data_t {
    uint32_t value{};
};
struct recent_mailbox_data_t {
    uint32_t value{};
};

using mailbox_data_t = std::variant<flags_mailbox_data_t,
                                    permanent_flags_mailbox_data_t,
                                    uidvalidity_data_t,
                                    exists_mailbox_data_t,
                                    recent_mailbox_data_t,
                                    unseen_resp_text_code_t,
                                    uidnext_resp_text_code_t,
                                    read_write_resp_text_code_t,
                                    read_only_resp_text_code_t,
                                    try_create_resp_text_code_t>;

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
