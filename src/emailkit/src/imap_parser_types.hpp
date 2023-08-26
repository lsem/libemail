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

///////////////////////////////////////////////////////////////////////////////////////////////
// mailbox-data
struct flags_mailbox_data_t {
    std::vector<std::string> flags_vec;
};
struct permanent_flags_mailbox_data_t {
    std::vector<std::string> flags_vec;
};
struct uidvalidity_data_t {
    uint32_t value{};
};
struct unseen_resp_text_code_t {
    uint32_t value;
};
struct uidnext_resp_text_code_t {
    uint32_t value{};
};
struct exists_mailbox_data_t {
    uint32_t value{};
};
struct recent_mailbox_data_t {
    uint32_t value{};
};
struct read_write_resp_text_code_t {};
struct read_only_resp_text_code_t {};
struct try_create_resp_text_code_t {};

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

///////////////////////////////////////////////////////////////////////////////////////////////
// message-data
struct msg_attr_envelope_t {
    std::string date;
    std::string subject;
    std::string from;
    std::string sender;
    std::string reply_to;
    std::string to;
    std::string cc;
    std::string bcc;
    std::string in_reply_to;
    std::string message_id;
};
struct msg_attr_uid_t {};
struct msg_attr_internaldate_t {};
struct msg_attr_body_structure_t {};
struct msg_attr_body_section_t {};
struct msg_attr_rfc822_t {};
struct msg_attr_rfc822_size_t {};

using msg_static_attr_t = std::variant<msg_attr_envelope_t,
                                       msg_attr_uid_t,
                                       msg_attr_internaldate_t,
                                       msg_attr_body_structure_t,
                                       msg_attr_body_section_t,
                                       msg_attr_rfc822_t,
                                       msg_attr_rfc822_size_t>;

// Roughly corresponds to message-data in the Grammar and represents any possible data for the
// message returned from fetch imap command. Because fetch may have different form, response itself
// can also have different form.
struct message_data_t {
    // std::vector<std::string> dynamic_attributes; // TODO: do we need special type for flag-fetch
    // here?
    uint32_t message_number{};
    std::vector<msg_static_attr_t> static_attrs;
};

namespace utils {
std::vector<std::string> decode_mailbox_path_from_list_response(const list_response_t& r);
}

std::string to_json(const list_response_t&);

}  // namespace emailkit::imap_parser
