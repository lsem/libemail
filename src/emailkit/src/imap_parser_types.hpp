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

struct Address {
    std::string addr_name;
    std::string addr_adl;
    std::string addr_mailbox;
    std::string addr_host;
};

struct Envelope {
    std::string date;
    std::string subject;
    std::vector<Address> from;
    std::vector<Address> sender;
    std::vector<Address> reply_to;
    std::vector<Address> to;
    std::vector<Address> cc;
    std::vector<Address> bcc;
    std::string in_reply_to;
    std::string message_id;
};

struct envelope_t {
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

struct msg_attr_uid_t {
    unsigned value;
};

using MsgAttrUID = msg_attr_uid_t;
struct msg_attr_internaldate_t {};
using msg_attr_envelope_t = envelope_t;

namespace standard_basic_media_types {
static const std::string application = "APPLICATION";
static const std::string audio = "AUDIO";
static const std::string image = "IMAGE";
static const std::string message = "MESSAGE";
static const std::string video = "VIDEO";
}  // namespace standard_basic_media_types

namespace standard_field_encodings {
static const std::string enc_7bit = "7BIT";
static const std::string enc_8bit = "8BIT";
static const std::string enc_binary = "BINARY";
static const std::string enc_base64 = "BASE64";
static const std::string enc_quoted_pritable = "QUOTED-PRINTABLE";
}  // namespace standard_field_encodings

using param_value_t = std::pair<std::string, std::string>;

using rfc822_headers_t = std::vector<std::pair<std::string, std::string>>;
using RFC822Headers = rfc822_headers_t;

namespace wip {
using namespace std;
struct BodyFields {
    std::vector<param_value_t> params;  // body-fld-param

    std::string field_id;    // body-fld-id
    std::string field_desc;  // body-fld-desc
    std::string encoding;    // body-fld-enc (see standard_field_encodings)
    uint32_t octets;         // body-fld-octets
};

// body-type-text  = media-text SP body-fields SP body-fld-lines
struct BodyTypeText {  // TODO: rename TextBody
    std::string media_subtype;
    BodyFields body_fields;
};

// body-type-basic = media-basic SP body-fields ; MESSAGE subtype MUST NOT be "RFC822"
struct BodyTypeBasic {  // TODO: rename BasicBody
    std::string media_type;
    std::string media_subtype;
    BodyFields body_fields;
};

// body-type-msg   = media-message SP body-fields SP envelope SP body SP body-fld-lines
struct BodyTypeMsg {};

struct BodyFieldDSP {
    std::string field_dsp_string;
    std::vector<param_value_t> field_params;
};

// body-ext-1part  = body-fld-md5 [SP body-fld-dsp [SP body-fld-lang [SP body-fld-loc *(SP
// body-extension)]]]
struct BodyExt1Part {
    std::string md5;
    BodyFieldDSP body_field_dsp;
};

// body-ext-mpart  = body-fld-param [SP body-fld-dsp [SP body-fld-lang [SP body-fld-loc *(SP
// body-extension)]]]
struct BodyExtMPart {
    std::vector<param_value_t> body_fld_params;
    BodyFieldDSP body_field_dsp;
};

class BodyType1Part;
class BodyTypeMPart;

using Body = variant<std::unique_ptr<BodyType1Part>, std::unique_ptr<BodyTypeMPart>>;

// body-type-1part = (body-type-text / body-type-basic / body-type-msg) [SP body-ext-1part]
struct BodyType1Part {
    variant<BodyTypeText, BodyTypeBasic, BodyTypeMsg> part_body;
    optional<BodyExt1Part> part_body_ext;
};

// body-type-mpart = 1*body SP media-subtype [SP body-ext-mpart]
struct BodyTypeMPart {
    std::vector<Body> body_ptrs;
    std::string media_subtype;
    optional<BodyExtMPart> multipart_body_ext;
};

}  // namespace wip

struct MsgAttrBodySection {};
struct MsgAttrRFC822 {
    std::string msg_data;
};
struct MsgAttrRFC822Size {
    uint32_t value = 0;
};

using MsgAttrStatic = std::variant<Envelope,
                                   msg_attr_uid_t,
                                   msg_attr_internaldate_t,
                                   wip::Body,
                                   MsgAttrBodySection,
                                   MsgAttrRFC822,
                                   MsgAttrRFC822Size>;

struct MessageData {
    uint32_t message_number = 0;
    std::vector<MsgAttrStatic> static_attributes;
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

// std::vector<std::pair<std::string, std::string>> params;  // body-fld-param

// std::string field_id;    // body-fld-id
// std::string field_desc;  // body-fld-desc
// std::string encoding;    // body-fld-enc (see standard_field_encodings)
// uint32_t octets{};       // body-fld-octets

// DEFINE_FMT_FORMATTER(emailkit::imap_parser::param_value_t,
//                      "({}, {})",
//                      std::get<0>(arg),
//                      std::get<1>(arg));

// DEFINE_FMT_FORMATTER(
//     emailkit::imap_parser::msg_attr_body_structure_t::body_fields_t,
//     "body_fields_t(params: [{}], field_id: {}, field_desc: {}, encoding: {}, octets: {})",
//     fmt::join(arg.params, ","),
//     arg.field_id,
//     arg.field_desc,
//     arg.encoding,
//     arg.octets);

// DEFINE_FMT_FORMATTER(emailkit::imap_parser::msg_attr_body_structure_t::body_type_text_t,p
//                      "body_type_text_t(media_subtype: {}, body_fields: {})",
//                      arg.media_subtype,
//                      arg.body_fields);

// DEFINE_FMT_FORMATTER(emailkit::imap_parser::msg_attr_body_structure_t::body_type_basic_t,
//                      "body_type_basic_t(media_type: {},media_subtype: {}, body_fields: {})",
//                      arg.media_type,
//                      arg.media_subtype,
//                      arg.body_fields);

// DEFINE_FMT_FORMATTER(emailkit::imap_parser::msg_attr_body_structure_t::body_ext_part_t,
//                      "body_ext_part_t()",
//                      arg.md5_or_fld_param);

// DEFINE_FMT_FORMATTER(
//     emailkit::imap_parser::msg_attr_body_structure_t::body_type_part,
//     "body_type_part(body_type: {})",
//     std::visit(
//         overload{[](const emailkit::imap_parser::msg_attr_body_structure_t::body_type_text_t& x)
//         {
//                      return fmt::format("{}", x);
//                  },
//                  [](const emailkit::imap_parser::msg_attr_body_structure_t::body_type_basic_t& x)
//                  {
//                      return fmt::format("{}", x);
//                  },
//                  [](const emailkit::imap_parser::msg_attr_body_structure_t::body_type_msg_t& x) {
//                      return fmt::format("msg");
//                  }},
//         arg.body_type));

// DEFINE_FMT_FORMATTER(emailkit::imap_parser::msg_attr_body_structure_t,
//                      "msg_attr_body_structure_t(parts: {})",
//                      arg.parts);

// DEFINE_FMT_FORMATTER(
//     emailkit::imap_parser::msg_static_attr_t,
//     "{}",
//     std::visit(overload{[](const emailkit::imap_parser::msg_attr_envelope_t& x) -> std::string {
//                             return "msg_attr_envelope_t";
//                         },
//                         [](const emailkit::imap_parser::msg_attr_uid_t& x) -> std::string {
//                             return "msg_attr_uid_t";
//                         },
//                         [](const emailkit::imap_parser::msg_attr_internaldate_t& x) ->
//                         std::string {
//                             return "msg_attr_internaldate_t";
//                         },
//                         [](const emailkit::imap_parser::msg_attr_body_structure_t& x)
//                             -> std::string { return fmt::format("{}", x); },
//                         [](const emailkit::imap_parser::msg_attr_body_section_t& x) ->
//                         std::string {
//                             return "msg_attr_body_section_t";
//                         },
//                         [](const emailkit::imap_parser::msg_attr_rfc822_t& x) -> std::string {
//                             return "msg_attr_rfc822_t";
//                         },
//                         [](const emailkit::imap_parser::msg_attr_rfc822_size_t& x) -> std::string
//                         {
//                             return "msg_attr_rfc822_size_t";
//                         }},
//                arg));

// DEFINE_FMT_FORMATTER(emailkit::imap_parser::message_data_t,
//                      "message_data_t(message_number: {}, static_attrs: [{}])",
//                      arg.message_number,
//                      fmt::join(arg.static_attrs, ", "));
