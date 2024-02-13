#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <emailkit/imap_parser.hpp>

#include <gmime/gmime.h>
#include <fstream>

using namespace emailkit;
using namespace testing;

TEST(imap_parser_test, basic_test) {
    const std::string select_command_result =
        "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen $NotPhishing $Phishing)\r\n"
        "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen $NotPhishing "
        "$Phishing \\*)] Flags permitted.\r\n"
        "* OK [UIDVALIDITY 1] UIDs valid.\r\n"
        "* 9 EXISTS\r\n"
        "* 19 EXISTS\r\n"
        "* 0 RECENT\r\n"
        "* OK [UNSEEN 3] Unseen count.\r\n"
        "* OK [UIDNEXT 10] Predicted next UID.\r\n"
        "* OK [HIGHESTMODSEQ 1909]\r\n"  // Note, this ignored by parser (TODO: implement)
        "A3 OK [READ-ONLY] INBOX selected. (Success)\r\n";
    "A3 OK [READ-WRITE] INBOX selected. (Success)\r\n";  // Note, this is not seen by parser
                                                         // (TODO: separate test)

    auto records_or_err = imap_parser::parse_mailbox_data_records(select_command_result);
    ASSERT_TRUE(records_or_err);
    auto& records = *records_or_err;
    ASSERT_GT(records.size(), 8);

    ASSERT_TRUE(std::holds_alternative<imap_parser::flags_mailbox_data_t>(records[0]));
    EXPECT_THAT(std::get<imap_parser::flags_mailbox_data_t>(records[0]).flags_vec,
                ElementsAre("\\Answered", "\\Flagged", "\\Draft", "\\Deleted", "\\Seen",
                            "$NotPhishing", "$Phishing"));

    ASSERT_TRUE(std::holds_alternative<imap_parser::permanent_flags_mailbox_data_t>(records[1]));
    EXPECT_THAT(std::get<imap_parser::permanent_flags_mailbox_data_t>(records[1]).flags_vec,
                ElementsAre("\\Answered", "\\Flagged", "\\Draft", "\\Deleted", "\\Seen",
                            "$NotPhishing", "$Phishing", "\\*"));

    ASSERT_TRUE(std::holds_alternative<imap_parser::uidvalidity_data_t>(records[2]));
    EXPECT_THAT(std::get<imap_parser::uidvalidity_data_t>(records[2]).value, 1);

    ASSERT_TRUE(std::holds_alternative<imap_parser::exists_mailbox_data_t>(records[3]));
    EXPECT_THAT(std::get<imap_parser::exists_mailbox_data_t>(records[3]).value, 9);

    ASSERT_TRUE(std::holds_alternative<imap_parser::exists_mailbox_data_t>(records[4]));
    EXPECT_THAT(std::get<imap_parser::exists_mailbox_data_t>(records[4]).value, 19);

    ASSERT_TRUE(std::holds_alternative<imap_parser::recent_mailbox_data_t>(records[5]));
    EXPECT_THAT(std::get<imap_parser::recent_mailbox_data_t>(records[5]).value, 0);

    ASSERT_TRUE(std::holds_alternative<imap_parser::unseen_resp_text_code_t>(records[6]));
    EXPECT_THAT(std::get<imap_parser::unseen_resp_text_code_t>(records[6]).value, 3);

    ASSERT_TRUE(std::holds_alternative<imap_parser::uidnext_resp_text_code_t>(records[7]));
    EXPECT_THAT(std::get<imap_parser::uidnext_resp_text_code_t>(records[7]).value, 10);

    // ASSERT_TRUE(std::holds_alternative<imap_parser::uidnext_resp_text_code_t>(records[8]));
    // EXPECT_THAT(std::get<imap_parser::uidnext_resp_text_code_t>(records[8]).value, 10);

    ASSERT_TRUE(std::holds_alternative<imap_parser::read_only_resp_text_code_t>(records[8]));
    // ASSERT_TRUE(std::holds_alternative<imap_parser::read_write_resp_text_code_t>(records[9]));
}

TEST(imap_parser_test, bad_syntax_in_flags__1) {
    const std::string select_command_result =
        "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen $NotPhishing $Phishing\r\n"
        "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen $NotPhishing "
        "$Phishing \\*)] Flags permitted.\r\n"
        "* OK [UIDVALIDITY 1] UIDs valid.\r\n"
        "* 9 EXISTS\r\n"
        "* 19 EXISTS\r\n"
        "* 0 RECENT\r\n"
        "* OK [UNSEEN 3] Unseen count.\r\n"
        "* OK [UIDNEXT 10] Predicted next UID.\r\n"
        "* OK [HIGHESTMODSEQ 1909]\r\n"
        "A3 OK [READ-WRITE] INBOX selected. (Success)\r\n";

    auto records_or_err = imap_parser::parse_mailbox_data_records(select_command_result);
    ASSERT_FALSE(records_or_err);
}
TEST(imap_parser_test, bad_syntax_in_flags__unexpected_reply_line) {
    // replies should be either tagged or untagged (*) or continuation requests (+)
    const std::string select_command_result =
        "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen $NotPhishing $Phishing)\r\n"
        "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen $NotPhishing "
        "$Phishing \\*)] Flags permitted.\r\n"
        "* OK [UIDVALIDITY 1] UIDs valid.\r\n"
        " 9 EXISTS\r\n"  // no *
        "* 19 EXISTS\r\n"
        "* 0 RECENT\r\n"
        "* OK [UNSEEN 3] Unseen count.\r\n"
        "* OK [UIDNEXT 10] Predicted next UID.\r\n"
        "* OK [HIGHESTMODSEQ 1909]\r\n"
        "A3 OK [READ-WRITE] INBOX selected. (Success)\r\n";

    auto records_or_err = imap_parser::parse_mailbox_data_records(select_command_result);
    ASSERT_FALSE(records_or_err);
}

TEST(imap_parser_test, DISABLED_bad_syntax_in_flags__continuation_request) {
    // how we are supposed to deal with it? the problem is that in real world there should not be
    // lines after continuation request so we need to somehow process it and then process the rest
    // of server output until the end.
}

TEST(imap_parser_test, uid_validity_isolated) {
    const std::string select_command_result =
        "* OK [UIDVALIDITY 1] UIDs valid.\r\nA3 OK Success.\r\n";

    auto records_or_err = imap_parser::parse_mailbox_data_records(select_command_result);
    ASSERT_TRUE(records_or_err);
    EXPECT_EQ(std::get<imap_parser::uidvalidity_data_t>(records_or_err.value()[0]).value, 1);
}

// TODO: fixme
TEST(imap_parser_test, DISABLED_uid_validity_isolated__max_uint32_number) {
    const std::string select_command_result =
        "* OK [UIDVALIDITY 4294967295] UIDs valid.\r\nA3 OK Success.\r\n";

    auto records_or_err = imap_parser::parse_mailbox_data_records(select_command_result);
    ASSERT_TRUE(records_or_err);
    EXPECT_EQ(std::get<imap_parser::uidvalidity_data_t>(records_or_err.value()[0]).value,
              4294967295);
}

// TODO: fixme
TEST(imap_parser_test, DISABLED_uid_validity_isolated__zero_number) {
    const std::string select_command_result =
        "* OK [UIDVALIDITY 0] UIDs valid.\r\nA3 OK Success.\r\n";

    auto records_or_err = imap_parser::parse_mailbox_data_records(select_command_result);
    ASSERT_FALSE(records_or_err);
}

TEST(imap_parser_test, parse_message_data_records_envelope_multiple) {
    // response for "FETCH 1:3(ENVELOPE)"
    // clang-format off
    const std::string response = 
        "* 1 FETCH (ENVELOPE (\"Sat, 5 Aug 2023 14:53:18 +0300\" \"ping\" ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<CA+n06nmeSAV4S3c5JLVJK2+j-bykMviYe91BpAERzbvLCbayDQ@mail.gmail.com>\"))\r\n"
        "* 2 FETCH (ENVELOPE (\"Sat, 05 Aug 2023 11:54:58 GMT\" \"=?UTF-8?B?0KHQv9C+0LLRltGJ0LXQvdC90Y8g0YHQuA==?= =?UTF-8?B?0YHRgtC10LzQuCDQsdC10LfQv9C10LrQuA==?=\" ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<ouNKs_Oyk5g8HnHk9Dn-RQ@notifications.google.com>\"))\r\n"
        "* 3 FETCH (ENVELOPE (\"Sun, 06 Aug 2023 07:23:30 GMT\" \"=?UTF-8?B?0KHQv9C+0LLRltGJ0LXQvdC90Y8g0YHQuA==?= =?UTF-8?B?0YHRgtC10LzQuCDQsdC10LfQv9C10LrQuA==?=\" ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<wMSYaDuR1oRwRZSYEu4Nxw@notifications.google.com>\"))\r\n"
        "A4 OK Success\r\n";
    // clang-format on

    auto message_data_records_or_err = imap_parser::parse_message_data_records(response);
    ASSERT_TRUE(message_data_records_or_err);
    auto& message_data_records = *message_data_records_or_err;

    ASSERT_EQ(message_data_records.size(), 3);
    EXPECT_EQ(message_data_records[0].message_number, 1);
    EXPECT_EQ(message_data_records[1].message_number, 2);
    EXPECT_EQ(message_data_records[2].message_number, 3);

    // envelop is the only statis attribute we are expecting for each record
    ASSERT_EQ(message_data_records[0].static_attributes.size(), 1);
    ASSERT_EQ(message_data_records[1].static_attributes.size(), 1);
    ASSERT_EQ(message_data_records[2].static_attributes.size(), 1);

    ASSERT_TRUE(std::holds_alternative<imap_parser::Envelope>(
        message_data_records[0].static_attributes[0]));
    ASSERT_TRUE(std::holds_alternative<imap_parser::Envelope>(
        message_data_records[1].static_attributes[0]));
    ASSERT_TRUE(std::holds_alternative<imap_parser::Envelope>(
        message_data_records[2].static_attributes[0]));

    auto& env_attr_r1 =
        std::get<imap_parser::Envelope>(message_data_records[0].static_attributes[0]);
    auto& env_attr_r2 =
        std::get<imap_parser::Envelope>(message_data_records[1].static_attributes[0]);
    auto& env_attr_r3 =
        std::get<imap_parser::Envelope>(message_data_records[2].static_attributes[0]);

    EXPECT_EQ(env_attr_r1.date, "Sat, 5 Aug 2023 14:53:18 +0300");
    EXPECT_EQ(env_attr_r2.date, "Sat, 05 Aug 2023 11:54:58 GMT");
    EXPECT_EQ(env_attr_r3.date, "Sun, 06 Aug 2023 07:23:30 GMT");
}

TEST(imap_parser_test, parse_message_data_records_fetch_fast_test) {
    // clang-format off
    const std::string fetch_fast_result =
        "* 1 FETCH (FLAGS (\\Seen) INTERNALDATE \"05-Aug-2023 11:53:31 +0000\" RFC822.SIZE 5152)\r\n"
        "* 2 FETCH (FLAGS (\\Seen) INTERNALDATE \"05-Aug-2023 11:54:59 +0000\" RFC822.SIZE 13754)\r\n"
        "* 3 FETCH (FLAGS (\\Seen) INTERNALDATE \"06-Aug-2023 07:23:31 +0000\" RFC822.SIZE 13756)\r\n"
        "A4 OK Success\r\n";
    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_fast_result);
    ASSERT_TRUE(message_data_or_err);
}

// TODO: test for multiple full recorords.

TEST(imap_parser_test, parse_message_data_records_fetch_full_minimal_test) {
    // clang-format off
    const std::string fetch_full_result =
        "* 1 FETCH (BODY ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\") NIL NIL \"7BIT\" 2 1)(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") NIL NIL \"7BIT\" 27 1) \"ALTERNATIVE\") ENVELOPE (\"Sat, 5 Aug 2023 14:53:18 +0300\" \"ping\" ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<CA+n06nmeSAV4S3c5JLVJK2+j-bykMviYe91BpAERzbvLCbayDQ@mail.gmail.com>\") FLAGS (\\Seen) INTERNALDATE \"05-Aug-2023 11:53:31 +0000\" RFC822.SIZE 5152)\r\n"
        "A4 OK Success\r\n";

    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_full_result);
    ASSERT_TRUE(message_data_or_err);
}

TEST(imap_parser_test, parse_message_data_records_fetch_full_test) {
    // clang-format off
    const std::string fetch_full_result =
        "* 3 FETCH (BODY ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\" \"DELSP\" \"yes\" \"FORMAT\" \"flowed\") NIL NIL \"BASE64\" 1316 27)(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") NIL NIL \"QUOTED-PRINTABLE\" 6485 130) \"ALTERNATIVE\") ENVELOPE (\"Sun, 06 Aug 2023 07:23:30 GMT\" \"=?UTF-8?B?0KHQv9C+0LLRltGJ0LXQvdC90Y8g0YHQuA==?= =?UTF-8?B?0YHRgtC10LzQuCDQsdC10LfQv9C10LrQuA==?=\" ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<wMSYaDuR1oRwRZSYEu4Nxw@notifications.google.com>\") FLAGS (\\Seen) INTERNALDATE \"06-Aug-2023 07:23:31 +0000\" RFC822.SIZE 13756)\r\n"
        "A4 OK Success\r\n";
    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_full_result);
    ASSERT_TRUE(message_data_or_err);
}

TEST(imap_parser_test, parse_message_data_records_fetch_full_multiline_test) {
    // clang-format off
    const std::string fetch_full_result =
"* 1 FETCH (BODY ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\") NIL NIL \"7BIT\" 2 1)(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") NIL NIL \"7BIT\" 27 1) \"ALTERNATIVE\") ENVELOPE (\"Sat, 5 Aug 2023 14:53:18 +0300\" \"ping\" ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((\"Liubomyr\" NIL \"liubomyr.semkiv.test\" \"gmail.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<CA+n06nmeSAV4S3c5JLVJK2+j-bykMviYe91BpAERzbvLCbayDQ@mail.gmail.com>\") FLAGS (\\Seen) INTERNALDATE \"05-Aug-2023 11:53:31 +0000\" RFC822.SIZE 5152)\r\n"
"* 2 FETCH (BODY ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\" \"DELSP\" \"yes\" \"FORMAT\" \"flowed\") NIL NIL \"BASE64\" 1316 27)(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") NIL NIL \"QUOTED-PRINTABLE\" 6485 130) \"ALTERNATIVE\") ENVELOPE (\"Sat, 05 Aug 2023 11:54:58 GMT\" \"=?UTF-8?B?0KHQv9C+0LLRltGJ0LXQvdC90Y8g0YHQuA==?= =?UTF-8?B?0YHRgtC10LzQuCDQsdC10LfQv9C10LrQuA==?=\" ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<ouNKs_Oyk5g8HnHk9Dn-RQ@notifications.google.com>\") FLAGS (\\Seen) INTERNALDATE \"05-Aug-2023 11:54:59 +0000\" RFC822.SIZE 13754)\r\n"
"* 3 FETCH (BODY ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\" \"DELSP\" \"yes\" \"FORMAT\" \"flowed\") NIL NIL \"BASE64\" 1316 27)(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") NIL NIL \"QUOTED-PRINTABLE\" 6485 130) \"ALTERNATIVE\") ENVELOPE (\"Sun, 06 Aug 2023 07:23:30 GMT\" \"=?UTF-8?B?0KHQv9C+0LLRltGJ0LXQvdC90Y8g0YHQuA==?= =?UTF-8?B?0YHRgtC10LzQuCDQsdC10LfQv9C10LrQuA==?=\" ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((\"Google\" NIL \"no-reply\" \"accounts.google.com\")) ((NIL NIL \"liubomyr.semkiv.test2\" \"gmail.com\")) NIL NIL NIL \"<wMSYaDuR1oRwRZSYEu4Nxw@notifications.google.com>\") FLAGS (\\Seen) INTERNALDATE \"06-Aug-2023 07:23:31 +0000\" RFC822.SIZE 13756)\r\n"
"A4 OK Success\r\n";
    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_full_result);
    ASSERT_TRUE(message_data_or_err);
}

// TODO: test for multiple rfc822 recorords.
TEST(imap_parser_test, parse_message_data_records_fetch_rfc822) {
    // clang-format off
    const std::string fetch_rfc822_result =
        "* 1 FETCH (RFC822 {4}\r\ntest)\r\n"
        "A4 OK Success\r\n";

    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_rfc822_result);
    ASSERT_TRUE(message_data_or_err);
}

TEST(imap_parser_test, parse_message_data_records_fetch_rfc822__correct_size) {
    // clang-format off
    const std::string fetch_rfc822_result =
        "* 1 FETCH (RFC822 {4}\r\n1234)\r\n"
        "A4 OK Success\r\n";

    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_rfc822_result);
    ASSERT_TRUE(message_data_or_err);
}

TEST(imap_parser_test, parse_message_data_records_fetch_rfc822__bigger_size) {
    // clang-format off
    const std::string fetch_rfc822_result =
        "* 1 FETCH (RFC822 {4}\r\n12345)\r\n"
        "A4 OK Success\r\n";

    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_rfc822_result);
    ASSERT_FALSE(message_data_or_err);
}

TEST(imap_parser_test, parse_message_data_records_fetch_rfc822__smaller_size) {
    // clang-format off
    const std::string fetch_rfc822_result =
        "* 1 FETCH (RFC822 {4}\r\n123)\r\n"
        "A4 OK Success\r\n";

    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_rfc822_result);
    ASSERT_FALSE(message_data_or_err);
}

TEST(imap_parser_test, parse_message_data_records_fetch_rfc822__zero_size) {
    // clang-format off
    const std::string fetch_rfc822_result =
        "* 1 FETCH (RFC822 {0}\r\n)\r\n"
        "A4 OK Success\r\n";

    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(fetch_rfc822_result);
    ASSERT_TRUE(message_data_or_err);
}

TEST(imap_parser_test, rfc_822_header_gmail_basic_test) {
    // TODO: get rid of this, use imap_parser::initialize() instead of make parser stafeul.
    ::g_mime_init();

    std::ifstream gmail_msg_file("rfc822_gmail_msg.dat", std::ios_base::in);
    ASSERT_TRUE(gmail_msg_file);

    std::string gmail_msg{std::istreambuf_iterator<char>(gmail_msg_file),
                          std::istreambuf_iterator<char>()};

    auto message_data_or_err = imap_parser::parse_rfc822_message(gmail_msg);
    ASSERT_TRUE(message_data_or_err);
}

TEST(imap_parser_test_, parse_bodystructure_response) {
    const std::string response =
        "* 32 FETCH (BODYSTRUCTURE ((((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\") \"NIL\" NIL "
        "\"BASE64\" 664 14 NIL NIL NIL)(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") NIL NIL "
        "\"BASE64\" 1596 32 NIL NIL NIL) \"ALTERNATIVE\" (\"BOUNDARY\" "
        "\"00000000000021d4e20604ca2f3c\") NIL NIL)(\"IMAGE\" \"PNG\" (\"NAME\" \"image.png\") "
        "\"<ii_lm9l1man0>\" NIL \"BASE64\" 35050 NIL (\"ATTACHMENT\" (\"FILENAME\" \"image.png\")) "
        "NIL) \"RELATED\" (\"BOUNDARY\" \"00000000000021d4e30604ca2f3d\") NIL NIL)(\"APPLICATION\" "
        "\"OCTET-STREAM\" (\"NAME\" \"DSC07119.arw\") \"<f_lm9l2vv01>\" NIL \"BASE64\" 28385390 "
        "NIL (\"ATTACHMENT\" (\"FILENAME\" \"DSC07119.arw\")) NIL) \"MIXED\" (\"BOUNDARY\" "
        "\"00000000000021d4e40604ca2f3e\") NIL NIL))\r\n"
        "A4 OK Success\r\n";
    auto message_data_or_err = imap_parser::parse_message_data_records(response);
    ASSERT_TRUE(message_data_or_err);

    auto& message_data = *message_data_or_err;
    ASSERT_EQ(message_data.size(), 1);

    auto& message_data_item = message_data[0];
    EXPECT_EQ(message_data_item.message_number, 32);
    // body-structure is the only attribute in this response
    ASSERT_EQ(message_data_item.static_attributes.size(), 1);

    using namespace emailkit::imap_parser::wip;

    ASSERT_TRUE(std::holds_alternative<Body>(message_data_item.static_attributes[0]));
    const Body& body_attribute = std::get<Body>(message_data_item.static_attributes[0]);

    // We expect four parts in total: text/plain, text/html, image/png, application/octet-stream

    // 1
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BodyTypeMPart>>(body_attribute));

    auto& body_mpart1 = std::get<std::unique_ptr<BodyTypeMPart>>(body_attribute);

    EXPECT_EQ(body_mpart1->media_subtype, "MIXED");
    ASSERT_TRUE(body_mpart1->multipart_body_ext.has_value());
    EXPECT_EQ(body_mpart1->multipart_body_ext->body_field_dsp.field_dsp_string, "");
    EXPECT_EQ(body_mpart1->multipart_body_ext->body_field_dsp.field_params.size(), 0);

    ASSERT_EQ(body_mpart1->body_ptrs.size(), 2);

    // 2
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BodyTypeMPart>>(body_mpart1->body_ptrs[0]));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BodyType1Part>>(body_mpart1->body_ptrs[1]));

    // 2.1
    auto& body_mpart2_1 = std::get<std::unique_ptr<BodyTypeMPart>>(body_mpart1->body_ptrs[0]);
    auto& body_mpart2_2 = std::get<std::unique_ptr<BodyType1Part>>(body_mpart1->body_ptrs[1]);

    EXPECT_EQ(body_mpart2_1->media_subtype, "RELATED");
    ASSERT_TRUE(body_mpart2_1->multipart_body_ext.has_value());
    EXPECT_EQ(body_mpart2_1->multipart_body_ext->body_field_dsp.field_dsp_string, "");
    EXPECT_EQ(body_mpart2_1->multipart_body_ext->body_field_dsp.field_params.size(), 0);
    ASSERT_EQ(body_mpart2_1->body_ptrs.size(), 2);

    ASSERT_TRUE(
        std::holds_alternative<std::unique_ptr<BodyTypeMPart>>(body_mpart2_1->body_ptrs[0]));
    ASSERT_TRUE(
        std::holds_alternative<std::unique_ptr<BodyType1Part>>(body_mpart2_1->body_ptrs[1]));

    // 2.1.1
    auto& body_mpart2_1_1 = std::get<std::unique_ptr<BodyTypeMPart>>(body_mpart2_1->body_ptrs[0]);
    auto& body_mpart2_1_2 = std::get<std::unique_ptr<BodyType1Part>>(body_mpart2_1->body_ptrs[1]);

    EXPECT_EQ(body_mpart2_1_1->media_subtype, "ALTERNATIVE");
    ASSERT_TRUE(body_mpart2_1_1->multipart_body_ext.has_value());
    EXPECT_EQ(body_mpart2_1_1->multipart_body_ext->body_field_dsp.field_dsp_string, "");
    EXPECT_EQ(body_mpart2_1_1->multipart_body_ext->body_field_dsp.field_params.size(), 0);
    ASSERT_EQ(body_mpart2_1_1->body_ptrs.size(), 2);

    ASSERT_TRUE(
        std::holds_alternative<std::unique_ptr<BodyType1Part>>(body_mpart2_1_1->body_ptrs[0]));
    ASSERT_TRUE(
        std::holds_alternative<std::unique_ptr<BodyType1Part>>(body_mpart2_1_1->body_ptrs[1]));

    {
        auto& body_mpart2_1_1_1 =
            std::get<std::unique_ptr<BodyType1Part>>(body_mpart2_1_1->body_ptrs[0]);
        auto& body_mpart2_1_1_2 =
            std::get<std::unique_ptr<BodyType1Part>>(body_mpart2_1_1->body_ptrs[1]);

        {
            ASSERT_TRUE(std::holds_alternative<BodyTypeText>(body_mpart2_1_1_1->part_body));
            auto& body_mpart2_1_1_1_part_body =
                std::get<BodyTypeText>(body_mpart2_1_1_1->part_body);
            EXPECT_EQ(body_mpart2_1_1_1_part_body.media_subtype, "PLAIN");
            EXPECT_EQ(body_mpart2_1_1_1_part_body.body_fields.encoding, "BASE64");
            EXPECT_EQ(body_mpart2_1_1_1_part_body.body_fields.field_desc, "");
            EXPECT_EQ(body_mpart2_1_1_1_part_body.body_fields.field_id, "NIL");
            EXPECT_EQ(body_mpart2_1_1_1_part_body.body_fields.octets, 664);
            ASSERT_EQ(body_mpart2_1_1_1_part_body.body_fields.params.size(), 1);
            EXPECT_EQ(body_mpart2_1_1_1_part_body.body_fields.params[0].first, "\"CHARSET\"");
            EXPECT_EQ(body_mpart2_1_1_1_part_body.body_fields.params[0].second, "\"UTF-8\"");

            EXPECT_EQ(body_mpart2_1_1_1->part_body_ext->md5, "");
            EXPECT_EQ(body_mpart2_1_1_1->part_body_ext->body_field_dsp.field_dsp_string, "");
            EXPECT_EQ(body_mpart2_1_1_1->part_body_ext->body_field_dsp.field_params.size(), 0);
        }
        {
            ASSERT_TRUE(std::holds_alternative<BodyTypeText>(body_mpart2_1_1_2->part_body));
            auto& body_mpart2_1_1_2_part_body =
                std::get<BodyTypeText>(body_mpart2_1_1_2->part_body);
            EXPECT_EQ(body_mpart2_1_1_2_part_body.media_subtype, "HTML");
            EXPECT_EQ(body_mpart2_1_1_2_part_body.body_fields.encoding, "BASE64");
            EXPECT_EQ(body_mpart2_1_1_2_part_body.body_fields.field_desc, "");
            EXPECT_EQ(body_mpart2_1_1_2_part_body.body_fields.field_id, "");
            EXPECT_EQ(body_mpart2_1_1_2_part_body.body_fields.octets, 1596);
            ASSERT_EQ(body_mpart2_1_1_2_part_body.body_fields.params.size(), 1);
            EXPECT_EQ(body_mpart2_1_1_2_part_body.body_fields.params[0].first, "\"CHARSET\"");
            EXPECT_EQ(body_mpart2_1_1_2_part_body.body_fields.params[0].second, "\"UTF-8\"");

            EXPECT_EQ(body_mpart2_1_1_2->part_body_ext->md5, "");
            EXPECT_EQ(body_mpart2_1_1_2->part_body_ext->body_field_dsp.field_dsp_string, "");
            EXPECT_EQ(body_mpart2_1_1_2->part_body_ext->body_field_dsp.field_params.size(), 0);
        }
    }

    {
        ASSERT_TRUE(std::holds_alternative<BodyTypeBasic>(body_mpart2_1_2->part_body));
        auto& body_mpart2_1_2_part_body = std::get<BodyTypeBasic>(body_mpart2_1_2->part_body);

        EXPECT_EQ(body_mpart2_1_2_part_body.media_type, "IMAGE");
        EXPECT_EQ(body_mpart2_1_2_part_body.media_subtype, "PNG");
        EXPECT_EQ(body_mpart2_1_2_part_body.body_fields.encoding, "BASE64");
        EXPECT_EQ(body_mpart2_1_2_part_body.body_fields.field_desc, "");
        EXPECT_EQ(body_mpart2_1_2_part_body.body_fields.field_id, "<ii_lm9l1man0>");
        EXPECT_EQ(body_mpart2_1_2_part_body.body_fields.octets, 35050);
        ASSERT_EQ(body_mpart2_1_2_part_body.body_fields.params.size(), 1);
        EXPECT_EQ(body_mpart2_1_2_part_body.body_fields.params[0].first, "\"NAME\"");
        EXPECT_EQ(body_mpart2_1_2_part_body.body_fields.params[0].second, "\"image.png\"");

        EXPECT_EQ(body_mpart2_1_2->part_body_ext->md5, "");
        EXPECT_EQ(body_mpart2_1_2->part_body_ext->body_field_dsp.field_dsp_string,
                  "\"ATTACHMENT\"");
        ASSERT_EQ(body_mpart2_1_2->part_body_ext->body_field_dsp.field_params.size(), 1);
        EXPECT_EQ(body_mpart2_1_2->part_body_ext->body_field_dsp.field_params[0].first,
                  "\"FILENAME\"");
        EXPECT_EQ(body_mpart2_1_2->part_body_ext->body_field_dsp.field_params[0].second,
                  "\"image.png\"");
    }

    {
        ASSERT_TRUE(std::holds_alternative<BodyTypeBasic>(body_mpart2_2->part_body));
        auto& body_mpart2_2_part_body = std::get<BodyTypeBasic>(body_mpart2_2->part_body);

        EXPECT_EQ(body_mpart2_2_part_body.media_type, "APPLICATION");
        EXPECT_EQ(body_mpart2_2_part_body.media_subtype, "OCTET-STREAM");
        EXPECT_EQ(body_mpart2_2_part_body.body_fields.encoding, "BASE64");
        EXPECT_EQ(body_mpart2_2_part_body.body_fields.field_desc, "");
        EXPECT_EQ(body_mpart2_2_part_body.body_fields.field_id, "<f_lm9l2vv01>");
        EXPECT_EQ(body_mpart2_2_part_body.body_fields.octets, 28385390);
        ASSERT_EQ(body_mpart2_2_part_body.body_fields.params.size(), 1);
        EXPECT_EQ(body_mpart2_2_part_body.body_fields.params[0].first, "\"NAME\"");
        EXPECT_EQ(body_mpart2_2_part_body.body_fields.params[0].second, "\"DSC07119.arw\"");

        EXPECT_EQ(body_mpart2_2->part_body_ext->md5, "");
        EXPECT_EQ(body_mpart2_2->part_body_ext->body_field_dsp.field_dsp_string, "\"ATTACHMENT\"");
        ASSERT_EQ(body_mpart2_2->part_body_ext->body_field_dsp.field_params.size(), 1);
        EXPECT_EQ(body_mpart2_2->part_body_ext->body_field_dsp.field_params[0].first,
                  "\"FILENAME\"");
        EXPECT_EQ(body_mpart2_2->part_body_ext->body_field_dsp.field_params[0].second,
                  "\"DSC07119.arw\"");
    }
}

TEST(imap_parser_test_, uid) {
    // C: A3 fetch 1:* (bodystructure uid envelope)

    const std::string response = "* 2 FETCH (UID 2)\r\nA3 OK Success\r\n";

    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(response);
    ASSERT_TRUE(message_data_or_err);

    auto& message_data = *message_data_or_err;
    ASSERT_EQ(message_data.size(), 1);
}

TEST(imap_parser_test_, DISABLED_parse_bodystructure_uid_envelope) {  // Disabled because is not
                                                                      // working for some reason.
    // C: A3 fetch 1:* (bodystructure uid envelope)
    const std::string response =
        R"(* 5 FETCH (UID 5 ENVELOPE ("Mon, 31 Jul 2023 16:22:11 GMT" "=?UTF-8?B?0KHQv9C+0LLRltGJ0LXQvdC90Y8g0YHQuA==?= =?UTF-8?B?0YHRgtC10LzQuCDQsdC10LfQv9C10LrQuA==?=" (("Google" NIL "no-reply" "accounts.google.com")) (("Google" NIL "no-reply" "accounts.google.com")) (("Google" NIL "no-reply" "accounts.google.com")) ((NIL NIL "liubomyr.semkiv.test" "gmail.com")) NIL NIL NIL "<1hYnffoH3Obfnoeje5ecMw@notifications.google.com>") BODYSTRUCTURE (("TEXT" "PLAIN" ("CHARSET" "UTF-8" "DELSP" "yes" "FORMAT" "flowed") NIL NIL "BASE64" 1320 27 NIL NIL NIL)("TEXT" "HTML" ("CHARSET" "UTF-8") NIL NIL "QUOTED-PRINTABLE" 6486 130 NIL NIL NIL) "ALTERNATIVE" ("BOUNDARY" "0000000000002c939d0601cad5ca") NIL NIL)))"
        "\r\n"
        R"(* 19 FETCH (UID 19 ENVELOPE ("Tue, 13 Feb 2024 10:08:06 +0200" "=?UTF-8?B?0LvQuNGB0YIg0Lcg0LLQutC70LDQtNC10L3QvdGP0LzQuA==?=" (("Liubomyr" NIL "liubomyr.semkiv.test" "gmail.com")) (("Liubomyr" NIL "liubomyr.semkiv.test" "gmail.com")) (("Liubomyr" NIL "liubomyr.semkiv.test" "gmail.com")) (("Liubomyr" NIL "liubomyr.semkiv.test" "gmail.com")) NIL NIL NIL "<CA+n06n=V6FqCudRF0iO=-sc8ZFEMiDXGYcTrdTGH-irq=HhjVw@mail.gmail.com>") BODYSTRUCTURE ((("TEXT" "PLAIN" ("CHARSET" "UTF-8") NIL NIL "7BIT"v 2 1 NIL NIL NIL)("TEXT" "HTML" ("CHARSET" "UTF-8") NIL NIL "7BIT" 27 1 NIL NIL NIL) "ALTERNATIVE" ("BOUNDARY" "000000000000f5dbd206113ee45f") NIL NIL)("APPLICATION" "VND.OASIS.OPENDOCUMENT.SPREADSHEET" ("NAME" "metrics_all.xlsx_0.ods") "<f_lsk2zdg20>" NIL "BASE64" 158338 NIL ("ATTACHMENT" ("FILENAME" "metrics_all.xlsx_0.ods")) NIL) "MIXED" ("BOUNDARY" "000000000000f5dbd406113ee461") NIL NIL)))"
        "\r\n"
        "A3 OK Success\r\n";
    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data_records(response);
    ASSERT_TRUE(message_data_or_err);

    auto& message_data = *message_data_or_err;
    ASSERT_EQ(message_data.size(), 1);
}

// TODO: Test for when field param is NIL.
