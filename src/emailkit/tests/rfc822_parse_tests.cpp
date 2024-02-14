#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <emailkit/imap_parser__rfc822.hpp>

#include <gmime/gmime.h>
#include <fstream>

using namespace emailkit;
using namespace emailkit::imap_parser;

class rfc822_parser_tests : public ::testing::Test {
   public:
    static void SetUpTestCase()  { ASSERT_TRUE(rfc822::initialize()); }
    static void TearDownTestCase()  { ASSERT_TRUE(rfc822::finalize()); }
};

TEST_F(rfc822_parser_tests, parse_headers_from_full_message_test) {
    std::ifstream gmail_msg_file("rfc822_gmail_msg.dat", std::ios_base::in);
    ASSERT_TRUE(gmail_msg_file);

    std::string gmail_msg{std::istreambuf_iterator<char>(gmail_msg_file),
                          std::istreambuf_iterator<char>()};

    auto headers_or_err = rfc822::parse_headers_from_rfc822_message(gmail_msg);
    ASSERT_TRUE(headers_or_err);

    auto& headers = *headers_or_err;
}

TEST_F(rfc822_parser_tests, parse_headers_unicode_subject_test) {
    std::ifstream gmail_msg_file("rfc822_gmail_msg__utf8_subject.dat", std::ios_base::in);
    ASSERT_TRUE(gmail_msg_file);

    std::string gmail_msg{std::istreambuf_iterator<char>(gmail_msg_file),
                          std::istreambuf_iterator<char>()};

    auto headers_or_err = rfc822::parse_headers_from_rfc822_message(gmail_msg);
    ASSERT_TRUE(headers_or_err);

    auto& headers = *headers_or_err;
}

TEST_F(rfc822_parser_tests, parse_headers_massive_gmail_pack_test) {
    std::ifstream gmail_msg_file("rfc822_gmail_headers_massive_pack.dat", std::ios_base::in);
    ASSERT_TRUE(gmail_msg_file);

    std::string gmail_msg{std::istreambuf_iterator<char>(gmail_msg_file),
                          std::istreambuf_iterator<char>()};

    auto headers_or_err = rfc822::parse_headers_from_rfc822_message(gmail_msg);
    ASSERT_TRUE(headers_or_err);

    auto& headers = *headers_or_err;
}
