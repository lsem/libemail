#pragma once

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
    std::string subject;
    EmailDate date;
    std::vector<EmailAddress> from;
    std::vector<EmailAddress> to;
    std::vector<EmailAddress> cc;
    std::vector<EmailAddress> bcc;
    std::vector<EmailAddress> sender;
    std::vector<EmailAddress> reply_to;

    // Non-mandatroy headers
    std::optional<MessageID> message_id;
    std::optional<MessageID> in_reply_to;
    std::optional<std::vector<MessageID>> references;

    std::map<std::string, std::string> raw_headers;

    std::vector<Attachment> attachments;
};

std::string to_json(const MailboxEmail& mail);

}  // namespace emailkit::types
