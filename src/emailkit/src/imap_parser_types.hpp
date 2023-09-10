#pragma once

#include <map>
#include <optional>
#include <string>
#include <system_error>
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
    // Date in format defined in rfc2822.
    // optional because server may return NIL for this field.
    std::optional<std::string> date_opt;

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

namespace standard_basic_media_types {
static const std::string application = "APPLICATION";
static const std::string audio = "AUDIO";
static const std::string image = "IMAGE";
static const std::string message = "MESSAGE";
static const std::string video = "VIDEO";
}  // namespace standard_basic_media_types

struct msg_attr_body_structure_t {
    struct body_type_text_t {
        // media_type is "TEXT" here.
        std::string media_subtype;
    };
    struct body_type_basic_t {
        std::string media_type;  // see standard_basic_media_types
        std::string media_subtype;
    };
    struct body_type_msg_t {};
    struct body_ext_part_t {};

    struct body_type_part {
        std::variant<body_type_text_t, body_type_basic_t, body_type_msg_t> body_type;
        std::optional<body_ext_part_t> ext_part;
    };

    std::vector<body_type_part> parts;
};
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

struct envelope_fields_t {
    std::optional<std::string> date_opt;
    std::optional<std::string> subject_opt;
    std::optional<std::string> from_opt;
    std::optional<std::string> sender_opt;
    std::optional<std::string> reply_to_opt;
    std::optional<std::string> to_opt;
    std::optional<std::string> cc_opt;
    std::optional<std::string> bcc_opt;
    std::optional<std::string> in_reply_to_opt;
    std::optional<std::string> message_id_opt;
};

namespace envelope_fields {
static const std::string date = "Date";
static const std::string subject = "Subject";
static const std::string from = "From";
static const std::string sender = "Sender";
static const std::string reply_to = "Reply-To";
static const std::string to = "To";
static const std::string cc = "Cc";
static const std::string bcc = "Bcc";
static const std::string message_id = "Message-ID";
static const std::string delivered_to = "Delivered-To";
static const std::string received = "Received";
static const std::string x_received = "X-Received";
static const std::string arc_seal = "ARC-Seal";
static const std::string arc_message_signature = "ARC-Message-Signature";
static const std::string arc_authentication_results = "ARC-Authentication-Results";
static const std::string return_path = "Return-Path";
static const std::string received_spf = "Received-SPF";
static const std::string authentication_results = "Authentication-Results";
static const std::string dkim_signature = "DKIM-Signature";
static const std::string x_google_dkim_signature = "X-Google-DKIM-Signature";
static const std::string x_gm_message_state = "X-Gm-Message-State";
static const std::string x_google_smtp_source = "X-Google-Smtp-Source";
static const std::string mime_version = "MIME-Version";
}  // namespace envelope_fields

using rfc822_headers_t = std::vector<std::pair<std::string, std::string>>;

namespace utils {
std::vector<std::string> decode_mailbox_path_from_list_response(const list_response_t& r);
}

std::string to_json(const list_response_t&);

}  // namespace emailkit::imap_parser

DEFINE_FMT_FORMATTER(
    emailkit::imap_parser::envelope_fields_t,
    "envelope_fields_t(date: {}, subject: {}, from: {}, sender: {}, reply_to: {}, to: {}, cc: {}, "
    "bcc: {}, in_repy_to: {}, message_id: {})",
    arg.date_opt.has_value() ? *arg.date_opt : "null",
    arg.subject_opt.has_value() ? *arg.subject_opt : "null",
    arg.from_opt.has_value() ? *arg.from_opt : "null",
    arg.sender_opt.has_value() ? *arg.sender_opt : "null",
    arg.reply_to_opt.has_value() ? *arg.reply_to_opt : "null",
    arg.to_opt.has_value() ? *arg.to_opt : "null",
    arg.cc_opt.has_value() ? *arg.cc_opt : "null",
    arg.bcc_opt.has_value() ? *arg.bcc_opt : "null",
    arg.in_reply_to_opt.has_value() ? *arg.in_reply_to_opt : "null",
    arg.message_id_opt.has_value() ? *arg.message_id_opt : "null");
