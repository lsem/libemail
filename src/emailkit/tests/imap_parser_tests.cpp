#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <emailkit/imap_parser.hpp>

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
    "A3 OK [READ-WRITE] INBOX selected. (Success)\r\n";  // Note, this is not seen by parser (TODO:
                                                         // separate test)

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
    ASSERT_EQ(message_data_records[0].static_attrs.size(), 1);
    ASSERT_EQ(message_data_records[1].static_attrs.size(), 1);
    ASSERT_EQ(message_data_records[2].static_attrs.size(), 1);

    ASSERT_TRUE(std::holds_alternative<imap_parser::msg_attr_envelope_t>(
        message_data_records[0].static_attrs[0]));
    ASSERT_TRUE(std::holds_alternative<imap_parser::msg_attr_envelope_t>(
        message_data_records[1].static_attrs[0]));
    ASSERT_TRUE(std::holds_alternative<imap_parser::msg_attr_envelope_t>(
        message_data_records[2].static_attrs[0]));

    auto& env_attr_r1 =
        std::get<imap_parser::msg_attr_envelope_t>(message_data_records[0].static_attrs[0]);
    auto& env_attr_r2 =
        std::get<imap_parser::msg_attr_envelope_t>(message_data_records[1].static_attrs[0]);
    auto& env_attr_r3 =
        std::get<imap_parser::msg_attr_envelope_t>(message_data_records[2].static_attrs[0]);

    EXPECT_EQ(*env_attr_r1.date_opt, "\"Sat, 5 Aug 2023 14:53:18 +0300\"");
    EXPECT_EQ(*env_attr_r2.date_opt, "\"Sat, 05 Aug 2023 11:54:58 GMT\"");
    EXPECT_EQ(*env_attr_r3.date_opt, "\"Sun, 06 Aug 2023 07:23:30 GMT\"");
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
    // clang-format off
    const std::string response =
"* 1 FETCH (RFC822.HEADER {4580}\r\n"
"Delivered-To: liubomyr.semkiv.test2@gmail.com\r\n"
"Received: by 2002:a05:6359:618c:b0:139:b597:11a4 with SMTP id\r\n"
 "sb12csp483616rwb; Sat, 5 Aug 2023 04:53:31 -0700 (PDT)\r\n"
"X-Received: by 2002:a05:651c:39c:b0:2b9:c689:3c33 with SMTP id\r\n"
 "e28-20020a05651c039c00b002b9c6893c33mr678546ljp.19.1691236411115; Sat, 05 Aug\r\n"
 "2023 04:53:31 -0700 (PDT)\r\n"
"ARC-Seal: i=1; a=rsa-sha256; t=1691236411; cv=none; d=google.com;\r\n"
 "s=arc-20160816;\r\n"
 "b=HsD8LYv68hwM9n1uup6koeVlbXFeEdkFUpNbQ2lBs4aX5i/fppjQWOEz9YNnUuGtbI\r\n"
 "7OceRJnuqupRmOg6qTibn3tIWyNkxY3fuQGi6RdKviDlLJyN82OQMFxXh6uSCigp7eZk\r\n"
 "2CB52X7LEutgQJva+vqhGC+iRTjiaaQNj29b039cFHpz9ZG5epSvYGPXrSKOOnxHphmE\r\n"
 "rj+0SbEwEpILC5mm5vQewUc+0QpHBMP+12Vmt76PDfcjwiLTvj2tu0eX9IbXgOSNb3s+\r\n"
 "r34HHBVjlD/ueiWC25RoA0gCUNEWmlANqM74040wuzSBppLhyEP7CS9hFSHb0s5u/4W5 uXRg==\r\n"
"ARC-Message-Signature: i=1; a=rsa-sha256; c=relaxed/relaxed; d=google.com;\r\n"
 "s=arc-20160816;\r\n"
 "h=to:subject:message-id:date:from:mime-version:dkim-signature;\r\n"
 "bh=JFxxozIgpXUl2G9pchuaut6R6KmtZePET7CU/x3Ho8Y=;\r\n"
 "fh=CvGPcTZEAlaEAtCmTmWhiJD69bS3CsYhvP1u19Vpc0s=;\r\n"
 "b=rmbq2KoM9gzGEzWr5B6XXgC75NaH+pdbTEIHNo41MwKiApkzidYrb26f8EcxvqaDJu\r\n"
 "DIkpq/reMBdvC1K4H+4i8H0gI6rb5Nzf4EBU31YStf6GSSPFaSpqJ93JrEE/YC3R+brs\r\n"
 "jxUf7C31yzYP5A6Ho/ViOLi5inl4jaUgmYVM314t1FtRI9ttLk90DTUdVEqAojaCtEw6\r\n"
 "CvD1FSO0nLnGFweLxnRA70i/iPBenQasbJEQEbrYpmHsajPyPIJB057wkr7Pk7kYcSAP\r\n"
 "9mDzRkZ7OMn20RjQ3kjNxh9u3m4CShV3HFpuec0Fo84C5cLEwocwicN8ktCeBD8bTQOJ UHig==\r\n"
"ARC-Authentication-Results: i=1; mx.google.com; dkim=pass header.i=@gmail.com\r\n"
 "header.s=20221208 header.b=KPVQ1N0L; spf=pass (google.com: domain of\r\n"
 "liubomyr.semkiv.test@gmail.com designates 209.85.220.41 as permitted sender)\r\n"
 "smtp.mailfrom=liubomyr.semkiv.test@gmail.com; dmarc=pass (p=NONE\r\n"
 "sp=QUARANTINE dis=NONE) header.from=gmail.com\r\n"
"Return-Path: <liubomyr.semkiv.test@gmail.com>\r\n"
"Received: from mail-sor-f41.google.com (mail-sor-f41.google.com.\r\n"
 "[209.85.220.41]) by mx.google.com with SMTPS id\r\n"
 "bd5-20020a05651c168500b002b734c4fe32sor16238ljb.1.2023.08.05.04.53.30 for\r\n"
 "<liubomyr.semkiv.test2@gmail.com> (Google Transport Security); Sat, 05 Aug\r\n"
 "2023 04:53:31 -0700 (PDT)\r\n"
"Received-SPF: pass (google.com: domain of liubomyr.semkiv.test@gmail.com\r\n"
 "designates 209.85.220.41 as permitted sender) client-ip=209.85.220.41;\r\n"
"Authentication-Results: mx.google.com; dkim=pass header.i=@gmail.com\r\n"
 "header.s=20221208 header.b=KPVQ1N0L; spf=pass (google.com: domain of\r\n"
 "liubomyr.semkiv.test@gmail.com designates 209.85.220.41 as permitted sender)\r\n"
 "smtp.mailfrom=liubomyr.semkiv.test@gmail.com; dmarc=pass (p=NONE\r\n"
 "sp=QUARANTINE dis=NONE) header.from=gmail.com\r\n"
"DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed; d=gmail.com; s=20221208;\r\n"
 "t=1691236410; x=1691841210;\r\n"
 "h=to:subject:message-id:date:from:mime-version:from:to:cc:subject\r\n"
 ":date:message-id:reply-to; bh=JFxxozIgpXUl2G9pchuaut6R6KmtZePET7CU/x3Ho8Y=;\r\n"
 "b=KPVQ1N0L+gjLhoKN1ospte5uVmcVyw5VGdKgsfir/NIoxR9vAFmICi/0VTBJnd7HEg\r\n"
 "oU7GxBEIIE/uyG06ixGSh5hz/jnQozcttcI/an81WO3chJUM5jcyJ1AaHX+ELhrmVSjh\r\n"
 "HAEwhdQ74wjvE8uFBwgZYwLEhLY7vFIr7VibwmFbayzSf4kBMg22F/CWU4taBkmUeXtq\r\n"
 "KRCurMKJyBEIef+FmxBrehdDejbNZpoq2qZWZtDl61QvjKCzLlq6XFQam6yArqRCRy30\r\n"
 "dQvPCVmjH/l4Lm/t3dcAMOVgEX56a0nvw9tGX3ZwFzqCqldCwFOQLYt+i5SmiA1j+tNR h1Iw==\r\n"
"X-Google-DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed; d=1e100.net;\r\n"
 "s=20221208; t=1691236410; x=1691841210;\r\n"
 "h=to:subject:message-id:date:from:mime-version:x-gm-message-state\r\n"
 ":from:to:cc:subject:date:message-id:reply-to;\r\n"
 "bh=JFxxozIgpXUl2G9pchuaut6R6KmtZePET7CU/x3Ho8Y=;\r\n"
 "b=J93WeP/gAzOAVMb1xJgt4miNlgG46EJ95CKps/Cz0nh2VUGlxqNgNCVXKswIgIvmm2\r\n"
 "cQbxa5bZi3Uod8tdbsZqw7ChuEdRxlHnyAJVhdfA9FqhYKIn1ckEkryk4kAebCcaPV8T\r\n"
 "1QgsVEn69CPlh7cOo6boRpt28IS86cFmfcf86tl3z+IDgU7voDcpEET4VBWQQbQqFZfA\r\n"
 "mby5gnphEXeuYmtFUbXRLA3+HG/xYKz6ByzGGBO33BrruCf2bxAZmoG9gJrm4b6Mttqf\r\n"
 "lVBlOebRXTLFXzod+xcd/tXAM+2Knl+kHcRM6VW9lARTk0F4og0ua7kI51AbGFgzR8+C ddEQ==\r\n"
"X-Gm-Message-State: AOJu0YxfBMjkwoEViBnNhHLXxv+4gh9v2HRqBwac2zggy7AD2ZU3ta9J\r\n"
 "BysVrtxs0nRf+oxwKZZEAgC55pfPiE8v07eqFFwiXT/8UbE=\r\n"
"X-Google-Smtp-Source: AGHT+IGPCwUHe4Pq1azoeN0Rf/sHJj8pmfGXkqUqzAtfHlMSAFP6hqxs2nzknhqeT2jwupNHyYDvp8kFO+QRJDDhoFY=\r\n"
"X-Received: by 2002:a2e:9690:0:b0:2b9:344c:181e with SMTP id\r\n"
 "q16-20020a2e9690000000b002b9344c181emr748168lji.19.1691236409950; Sat, 05 Aug\r\n"
 "2023 04:53:29 -0700 (PDT)\r\n"
"MIME-Version: 1.0\r\n"
"From: Liubomyr <liubomyr.semkiv.test@gmail.com>\r\n"
"Date: Sat, 5 Aug 2023 14:53:18 +0300\r\n"
"Message-ID: <CA+n06nmeSAV4S3c5JLVJK2+j-bykMviYe91BpAERzbvLCbayDQ@mail.gmail.com>\r\n"
"Subject: ping\r\n"
"To: liubomyr.semkiv.test2@gmail.com\r\n"
"Content-Type: multipart/alternative; boundary=\"0000000000007245a806022ba9cd\"\r\n"
"\r\n"
")\r\n"
"A4 OK Success\r\n";
    // clang-format on

    auto message_data_or_err = imap_parser::parse_message_data_records(response);
    ASSERT_TRUE(message_data_or_err);
}


