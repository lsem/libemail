
#include "types.hpp"
#include <fmt/format.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace emailkit::types {

using namespace rapidjson;

void encode_string_vec(Writer<StringBuffer>& writer, const std::vector<EmailAddress>& vec) {
    writer.StartArray();
    for (auto& a : vec) {
        writer.String(a.c_str());
    }
    writer.EndArray();
}

void encode_mail_adress_vec(Writer<StringBuffer>& writer, const std::vector<EmailAddress>& addr) {
    encode_string_vec(writer, addr);
}

std::string to_json(const MailboxEmail& mail) {
    StringBuffer s;
    PrettyWriter<StringBuffer> writer(s);

    writer.StartObject();

    writer.Key("subject");
    writer.String(mail.subject.c_str());

    // ISO8601
    writer.Key("date");
    writer.String(fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", mail.date.year,
                              mail.date.month, mail.date.day, mail.date.hours, mail.date.minutes,
                              mail.date.seconds)
                      .c_str());

    writer.Key("from");
    encode_mail_adress_vec(writer, mail.from);

    writer.Key("to");
    encode_mail_adress_vec(writer, mail.to);

    writer.Key("cc");
    encode_mail_adress_vec(writer, mail.cc);

    writer.Key("bcc");
    encode_mail_adress_vec(writer, mail.bcc);

    writer.Key("sender");
    encode_mail_adress_vec(writer, mail.sender);

    writer.Key("reply_to");
    encode_mail_adress_vec(writer, mail.reply_to);

    writer.Key("message_id");
    if (mail.message_id.has_value()) {
        writer.String(mail.message_id->c_str());
    } else {
        writer.Null();
    }

    writer.Key("in_reply_to");
    if (mail.in_reply_to.has_value()) {
        writer.String(mail.in_reply_to->c_str());
    } else {
        writer.Null();
    }

    writer.Key("references");
    if (mail.references.has_value()) {
        encode_string_vec(writer, *mail.references);
    } else {
        writer.Null();
    }

    writer.Key("raw_headers");
    writer.StartObject();
    for (auto& [h, v] : mail.raw_headers) {
        writer.Key(h.c_str());
        writer.String(v.c_str());
    }
    writer.EndObject();

    writer.Key("attachments");
    writer.StartArray();
    for (auto& x : mail.attachments) {
        writer.StartObject();
        writer.Key("type");
        writer.String(x.type.c_str());
        writer.Key("subtype");
        writer.String(x.subtype.c_str());
        writer.Key("name");
        writer.String(x.name.c_str());
        writer.Key("octets");
        writer.Int(x.octets);
        writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();

    return s.GetString();
}
}  // namespace emailkit::types
