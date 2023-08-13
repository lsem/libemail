#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <emailkit/imap_response_line.hpp>

TEST(imap_response_line_test, basic_test) {
    emailkit::imap_response_line_t response_line{"T0 OK [AUTHENTICATED]\r\n"};
    EXPECT_EQ(response_line.line, "T0 OK [AUTHENTICATED]\r\n");
    EXPECT_THAT(response_line.tokens, ::testing::ElementsAre("T0", "OK", "[AUTHENTICATED]"));
    EXPECT_EQ(response_line.is_untagged_reply(), false);
}

TEST(imap_response_line_test, empty_line) {
    emailkit::imap_response_line_t response_line{"\r\n"};
    EXPECT_EQ(response_line.line, "\r\n");
    EXPECT_THAT(response_line.tokens, ::testing::ElementsAre());
    EXPECT_EQ(response_line.is_untagged_reply(), false);
}

TEST(imap_response_line_test, untagged_reply) {
    emailkit::imap_response_line_t response_line{"* something replies without tag\r\n"};
    EXPECT_EQ(response_line.is_untagged_reply(), true);
    EXPECT_EQ(response_line.is_command_continiation_request(), false);
}

TEST(imap_response_line_test, continutation_request) {
    emailkit::imap_response_line_t response_line{"+ something replies without tag\r\n"};
    EXPECT_EQ(response_line.is_untagged_reply(), false);
    EXPECT_EQ(response_line.is_command_continiation_request(), true);
}

TEST(imap_response_line_test, untagged_reply_utils) {
    {
        emailkit::imap_response_line_t response_line{R"(* LIST (\HasNoChildren) "/" "INBOX"\r\n)"};
        EXPECT_TRUE(response_line.is_untagged_reply());
        EXPECT_EQ(response_line.unwrap_untagged_reply(),
                  R"(LIST (\HasNoChildren) "/" "INBOX"\r\n)");
    }
    {
        // emailkit::imap_response_line_t response_line{R"(* \r\n)"};
        // EXPECT_TRUE(response_line.is_untagged_reply());
        // EXPECT_EQ(response_line.unwrap_untagged_reply(),
        //           R"(LIST (\HasNoChildren) "/" "INBOX"\r\n)");
    }
}
