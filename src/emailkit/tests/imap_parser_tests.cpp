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

TEST(imap_parser_test, parse_message_data_basic_test) {
    // clang-format off
    const std::string fetch_result =
        "* 1 FETCH (FLAGS (\\Seen) INTERNALDATE \"05-Aug-2023 11:53:31 +0000\" RFC822.SIZE 5152)\r\n"
        "* 2 FETCH (FLAGS (\\Seen) INTERNALDATE \"05-Aug-2023 11:54:59 +0000\" RFC822.SIZE 13754)\r\n"
        "* 3 FETCH (FLAGS (\\Seen) INTERNALDATE \"06-Aug-2023 07:23:31 +0000\" RFC822.SIZE 13756)\r\n"
        "A4 OK Success\r\n";
    // clang-format on
    auto message_data_or_err = imap_parser::parse_message_data(fetch_result);
    ASSERT_TRUE(message_data_or_err);
}
