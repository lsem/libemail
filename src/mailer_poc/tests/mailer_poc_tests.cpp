#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mailer_ui_state.hpp>
#include <sstream>

using namespace emailkit;
using emailkit::imap_client::types::list_response_entry_t;
using emailkit::types::EmailAddress;
using emailkit::types::MessageID;

string render_tree(const mailer::MailerUIState& ui_state, bool extra = false) {
    std::stringstream ss;

    int indentation = 0;
    string indent_str;

    ui_state.walk_tree_preoder(
        [&](auto& folder) {  // entered
            ss << indent_str << "[" << folder << "]\n";
            indent_str += "    ";
        },
        [&](auto& folder) {  // exited
            indent_str.resize(indent_str.size() - 4);
        },
        [&](auto& ref) {  // reference
            if (extra) {
                ss << fmt::format("{}{} (emails: {}{})\n", indent_str,
                                  (ref.label.empty() ? "<No-Subject>" : ref.label),
                                  ref.emails_count,
                                  ref.attachments_count > 0
                                      ? fmt::format(", attachments: {}", ref.attachments_count)
                                      : "");

            } else {
                ss << indent_str << ref.label << "\n";
            }
        });

    return ss.str();
}

auto make_email = [](vector<types::EmailAddress> from,
                     vector<types::EmailAddress> to,
                     auto subject,
                     auto message_id,
                     vector<MessageID> references = {}) {
    return types::MailboxEmail{.message_uid = 100,
                               .subject = subject,
                               .date =
                                   types::EmailDate{
                                       // ..
                                   },
                               .from = from,
                               .to = to,
                               .cc = vector<types::EmailAddress>{},
                               .bcc = vector<types::EmailAddress>{},
                               .sender = vector<types::EmailAddress>{},
                               .reply_to = vector<types::EmailAddress>{},
                               .message_id = message_id,
                               .in_reply_to = {},
                               .references = references,
                               .raw_headers = {},
                               .attachments = {}};
};

TEST(mailer_poc_tests, basic) {
    mailer::MailerUIState ui_state{"sli.ukraine@gmail.com"};

    ui_state.process_email(make_email({"sli.ukraine@gmail.com"}, {"combdn@gmail.com"}, "Hi", "e1"));
    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
)",
        render_tree(ui_state, true));

    ui_state.process_email(
        make_email({"combdn@gmail.com"}, {"sli.ukraine@gmail.com"}, "Money", "e2"));
    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
)",
        render_tree(ui_state, true));

    ui_state.process_email(
        make_email({"vasia@gmail.com"}, {"sli.ukraine@gmail.com"}, "Ski racing", "e3"));
    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
    [vasia@gmail.com]
        Ski racing (emails: 1)
)",
        render_tree(ui_state, true));

    ui_state.process_email(
        make_email({"combdn@gmail.com"}, {"sli.ukraine@gmail.com"}, "Different topic", "e4"));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
        Different topic (emails: 1)
    [vasia@gmail.com]
        Ski racing (emails: 1)
)",
        render_tree(ui_state, true));

    // Reply to one should not produce new conversations
    ui_state.process_email(make_email({"sli.ukraine@gmail.com"}, {"combdn@gmail.com"},
                                      "RE: Different topic", "e5", {"e4"}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
        Different topic (emails: 2)
    [vasia@gmail.com]
        Ski racing (emails: 1)
)",
        render_tree(ui_state, true));

    // Adding Vasia, now we have three people conversation
    ui_state.process_email(make_email({"sli.ukraine@gmail.com"},
                                      {"combdn@gmail.com", "vasia@gmail.com"},
                                      "RE: Different topic", "e6", {"e4", "e5"}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
    [vasia@gmail.com]
        Ski racing (emails: 1)
    [combdn@gmail.com, vasia@gmail.com]
        Different topic (emails: 3)
)",
        render_tree(ui_state, true));

    // Vasia replies but since this is still the same 3 people converstations the tree should
    // not change except number of emails.
    ui_state.process_email(make_email({"vasia@gmail.com"},
                                      {"combdn@gmail.com", "sli.ukraine@gmail.com"},
                                      "RE: Different topic", "e7", {"e4", "e5", "e6"}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
    [vasia@gmail.com]
        Ski racing (emails: 1)
    [combdn@gmail.com, vasia@gmail.com]
        Different topic (emails: 4)
)",
        render_tree(ui_state, true));

    // Now Vasia creates another topic first into Valerii
    ui_state.process_email(
        make_email({"vasia@gmail.com"}, {"sli.ukraine@gmail.com"}, "New Topic", "d1", {}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
    [vasia@gmail.com]
        Ski racing (emails: 1)
        New Topic (emails: 1)
    [combdn@gmail.com, vasia@gmail.com]
        Different topic (emails: 4)
)",
        render_tree(ui_state, true));

    // And then Vasia adds Valerii making a group of the same three people but with different
    // thread.
    ui_state.process_email(make_email({"vasia@gmail.com"},
                                      {"sli.ukraine@gmail.com", "combdn@gmail.com"},
                                      "RE: New Topic", "d2", {"d1"}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
    [vasia@gmail.com]
        Ski racing (emails: 1)
    [combdn@gmail.com, vasia@gmail.com]
        Different topic (emails: 4)
        New Topic (emails: 2)
)",
        render_tree(ui_state, true));

    ui_state.process_email(
        make_email({"sli.ukraine@gmail.com"}, {"vasia@gmail.com"}, "Give me the money", "f1", {}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Hi (emails: 1)
        Money (emails: 1)
    [vasia@gmail.com]
        Ski racing (emails: 1)
        Give me the money (emails: 1)
    [combdn@gmail.com, vasia@gmail.com]
        Different topic (emails: 4)
        New Topic (emails: 2)
)",
        render_tree(ui_state, true));
}

TEST(mailer_poc_tests, conversation_with_self_basic_test) {
    mailer::MailerUIState ui{"liubomyr.semkiv.test@gmail.com"};
    ui.process_email(make_email({"liubomyr.semkiv.test@gmail.com"},
                                {"liubomyr.semkiv.test@gmail.com"}, "Test Email",
                                "78C7A359-B513-496B-B140-66E5896DE6C4@gmail.com", {}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        Test Email
)",
        render_tree(ui));

    ui.process_email(make_email({"liubomyr.semkiv.test@gmail.com"},
                                {"liubomyr.semkiv.test@gmail.com"}, "Re: Test Email",
                                "88C7A359-B513-496B-B140-66E5896DE6C4@gmail.com",
                                {"78C7A359-B513-496B-B140-66E5896DE6C4@gmail.com"}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        Test Email
)",
        render_tree(ui));

    ui.process_email(make_email({"liubomyr.semkiv.test@gmail.com"},
                                {"liubomyr.semkiv.test@gmail.com"}, "Email with attachments",
                                "99C7A359-B513-496B-B140-66E5896DE6C4@gmail.com", {}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        Test Email
        Email with attachments
)",
        render_tree(ui));
}

TEST(mailer_poc_tests, conversation_with_self_real_world_issue) {
    mailer::MailerUIState ui{"liubomyr.semkiv.test@gmail.com"};
    ui.process_email(
        make_email({"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
                   "test email (from self)",
                   "CA+n06nnjA6eQOy5D+HUipuKfU=Psmss_t6wyvUA=z2g38GDvxQ@mail.gmail.com", {}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
)",
        render_tree(ui));

    ui.process_email(
        make_email({"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
                   "лист з вкладеннями",
                   "CA+n06n=V6FqCudRF0iO=-sc8ZFEMiDXGYcTrdTGH-irq=HhjVw@mail.gmail.com", {}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
        лист з вкладеннями
)",
        render_tree(ui));

    ui.process_email(make_email(
        {"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
        "Re: лист з вкладеннями",
        "CA+n06nm7ACHhP4tfA+C9za7eUU87J9AdmmF6yUTJ7hXGZpFqVw@mail.gmail.com",
        // in-reply-to: "<CA+n06n=V6FqCudRF0iO=-sc8ZFEMiDXGYcTrdTGH-irq=HhjVw@mail.gmail.com>",
        {"CA+n06n=V6FqCudRF0iO=-sc8ZFEMiDXGYcTrdTGH-irq=HhjVw@mail.gmail.com"}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
        лист з вкладеннями
)",
        render_tree(ui));

    ui.process_email(make_email(
        {"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
        "Re: лист з вкладеннями",
        "CA+n06nmovTcG3sHyS1yeOmDvy+wbcbQUypKPVRPnNTJJb0UD8A@mail.gmail.com",

        {"CA+n06n=V6FqCudRF0iO=-sc8ZFEMiDXGYcTrdTGH-irq=HhjVw@mail.gmail.com",
         // in-reply-to: "<CA+n06nm7ACHhP4tfA+C9za7eUU87J9AdmmF6yUTJ7hXGZpFqVw@mail.gmail.com>",
         "CA+n06nm7ACHhP4tfA+C9za7eUU87J9AdmmF6yUTJ7hXGZpFqVw@mail.gmail.com"}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
        лист з вкладеннями
)",
        render_tree(ui));

    ui.process_email(
        make_email({"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
                   "Re: лист з вкладеннями",
                   "CA+n06nmovTcG3sHyS1yeOmDvy+wbcbQUypKPVRPnNTJJb0UD8A@mail.gmail.com",
                   {"CA+n06n=V6FqCudRF0iO=-sc8ZFEMiDXGYcTrdTGH-irq=HhjVw@mail.gmail.com",
                    "CA+n06nm7ACHhP4tfA+C9za7eUU87J9AdmmF6yUTJ7hXGZpFqVw@mail.gmail.com"}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
        лист з вкладеннями
)",
        render_tree(ui));

    ui.process_email(
        make_email({"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
                   "Re: лист з вкладеннями",
                   "CA+n06nkivAy3ALNeO4PCzJWyHE+s_YczNoL3TP=+_=p9F+58ng@mail.gmail.com",
                   {"CA+n06n=V6FqCudRF0iO=-sc8ZFEMiDXGYcTrdTGH-irq=HhjVw@mail.gmail.com",
                    "CA+n06nm7ACHhP4tfA+C9za7eUU87J9AdmmF6yUTJ7hXGZpFqVw@mail.gmail.com",
                    "CA+n06nmovTcG3sHyS1yeOmDvy+wbcbQUypKPVRPnNTJJb0UD8A@mail.gmail.com"}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
        лист з вкладеннями
)",
        render_tree(ui));

    ui.process_email(make_email(
        {"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"}, "new test email",
        "CA+n06n=gaO4yss4F7H_AP2_iPdTDmGU2E0-41bhx3zxquwYbcw@mail.gmail.com", {}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
        лист з вкладеннями
        new test email
)",
        render_tree(ui));

    // This email looks like reply to previous, but in fact it is an email with crafted subject
    // but there are no in-reply-to or references. So we expect creation of new thread Re: new
    // test email.
    ui.process_email(
        make_email({"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
                   "Re: new test email",
                   "CA+n06nnjWWyY+QS2i5s9cphRwMTBqgiKbbp3epMsgcn0DiQN8w@mail.gmail.com", {}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self)
        лист з вкладеннями
        new test email
        Re: new test email
)",
        render_tree(ui));

    // And this is real reply to "new test email", so we don't expect creation of new thread.
    ui.process_email(make_email(
        {"liubomyr.semkiv.test@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
        "Re: new test email", "CA+n06nkGjQikcWQ_pVR1BqbL1M_igb5Ks-kbyG2vBHkeBV4hVg@mail.gmail.com",
        // In-Reply-To: "<CA+n06n=gaO4yss4F7H_AP2_iPdTDmGU2E0-41bhx3zxquwYbcw@mail.gmail.com>"
        {"CA+n06n=gaO4yss4F7H_AP2_iPdTDmGU2E0-41bhx3zxquwYbcw@mail.gmail.com"}));
    ASSERT_EQ(
        R"([root]
    [liubomyr.semkiv.test@gmail.com]
        test email (from self) (emails: 1)
        лист з вкладеннями (emails: 5)
        new test email (emails: 2)
        Re: new test email (emails: 1)
)",
        render_tree(ui, true));
}

TEST(mailer_poc_tests, real_workd_multi_group_test) {
    mailer::MailerUIState ui{"liubomyr.semkiv.test@gmail.com"};

    log_debug("ingesting first email");
    ui.process_email(make_email({"combdn@gmail.com"}, {"liubomyr.semkiv.test@gmail.com"},
                                "Testing multi-contact grouping",
                                "78C7A359-B513-496B-B140-66E5896DE6C4@gmail.com", {}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Testing multi-contact grouping
)",
        render_tree(ui));

    log_debug("ingesting second email");
    ui.process_email(
        make_email({"liubomyr.semkiv.test@gmail.com"}, {"combdn@gmail.com"},
                   "Re: Testing multi-contact grouping",
                   "CA+n06nmoc7=kOVqRs7kBAcapksps28M0Nkjsjbieebw9uK2fBg@mail.gmail.com",
                   {"78C7A359-B513-496B-B140-66E5896DE6C4@gmail.com"}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com]
        Testing multi-contact grouping
)",
        render_tree(ui));

    log_debug("ingesting third email");
    ui.process_email(make_email(
        {"combdn@gmail.com"}, {"liubomyr.semkiv.test@gmail.com", "sli.ukraine@gmail.com"},
        "Re: Testing multi-contact grouping", "E62F4DB8-BF75-47A2-B781-EC7DEE609EFE@gmail.com",
        {"78C7A359-B513-496B-B140-66E5896DE6C4@gmail.com",
         "CA+n06nmoc7=kOVqRs7kBAcapksps28M0Nkjsjbieebw9uK2fBg@mail.gmail.com"}));

    ASSERT_EQ(
        R"([root]
    [combdn@gmail.com, sli.ukraine@gmail.com]
        Testing multi-contact grouping
)",
        render_tree(ui));
}

    // How to find thread id?
