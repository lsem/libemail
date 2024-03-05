#pragma once

#include <emailkit/global.hpp>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace emailkit::types {

// Represents email address like user@example.com. This is native type for our library and apps.
using EmailAddress = std::string;

using EmailAddressVec = std::vector<EmailAddress>;

using MessageID = std::string;

// UTC date
struct EmailDate {
    int year;
    int month;
    int day;
    int hours;
    int minutes;
    int seconds;
};

struct Attachment {
    std::string type;
    std::string subtype;
    std::string name;
    uint32_t octets;
};

struct MailboxEmail {
    // IDs: id in this mailbox, message-ID (if this is standard, or if it is extension then it
    // should be optinal).x

    // https://datatracker.ietf.org/doc/html/rfc3501#section-2.3.1.1
    // Unique Identifier (UID) Message Attribute (2.3.1.1)
    int message_uid;

    // Mandatory headers
    string subject;
    EmailDate date;
    vector<EmailAddress> from;
    vector<EmailAddress> to;
    vector<EmailAddress> cc;
    vector<EmailAddress> bcc;
    vector<EmailAddress> sender;
    vector<EmailAddress> reply_to;

    // Non-mandatroy headers
    optional<MessageID> message_id;
    optional<MessageID> in_reply_to;
    optional<std::vector<MessageID>> references;

    map<string, string> raw_headers;

    vector<Attachment> attachments;
};

std::string to_json(const MailboxEmail& mail);

}  // namespace emailkit::types
