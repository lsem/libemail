#include "emailapp_core.hpp"
#include <emailkit/google_auth.hpp>
#include <emailkit/imap_client.hpp>

namespace emailapp::core {

// TODO: this is going to be public type to be cosumed by UI
struct EmailItemBodyPart {};

/// Represents email in the App. Email is downloaded from IMAP server and then can be put into cache
/// or displayed on the UI. It is a thing you see rendered in email list or a thing you open to see
/// its content.
struct EmailItem {
    unsigned uid = 0;

    struct {
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
    } envelope;

    struct {
        // We should support only normalized structure which does not allow trees but only lists.
        std::vector<EmailItemBodyPart> body_parts;
    } structure;
};

namespace {
using namespace emailkit::imap_parser::wip;
void traverse_body(const Body& body, int indent_width = 0) {
    std::string indent_str = std::string(indent_width, ' ');

    if (std::holds_alternative<std::unique_ptr<BodyType1Part>>(body)) {
        auto& one_part = *std::get<std::unique_ptr<BodyType1Part>>(body);

        if (auto* text_part = std::get_if<BodyTypeText>(&one_part.part_body)) {
            log_info("{}TEXT (subtype: {})", indent_str, text_part->media_subtype);
        } else if (auto* basic_part = std::get_if<BodyTypeBasic>(&one_part.part_body)) {
            log_info("{}BASIC (type: {}, subtype: {})", indent_str, basic_part->media_type,
                     basic_part->media_subtype);
        } else if (auto* msg_part = std::get_if<BodyTypeMsg>(&one_part.part_body)) {
            log_info("{}MESSAGE", indent_str);
        }
    } else {
        auto& multi_part = *std::get<std::unique_ptr<BodyTypeMPart>>(body);
        log_info("{}MULTI (subtype: {})", indent_str, multi_part.media_subtype);

        for (auto& subpart : multi_part.body_ptrs) {
            traverse_body(subpart, indent_width + 4);
        }

        log_info("{}END MULTI", indent_str);
    }
}
}  // namespace

class EmailAppCoreImpl : public EmailAppCore, public EnableUseThis<EmailAppCoreImpl> {
   public:
    explicit EmailAppCoreImpl(asio::io_context& ctx, EmailAppCoreCallbacks& cbs)
        : m_ctx(ctx), m_cbs(cbs), m_imap_client() {}

    bool initialize() {
        m_imap_client = emailkit::imap_client::make_imap_client(m_ctx);
        if (!m_imap_client) {
            log_error("failed creating imap client");
            return false;
        }

        m_google_auth = emailkit::make_google_auth(m_ctx, "127.0.0.1", "8089");
        if (!m_google_auth) {
            log_error("failed creating google auth");
            return false;
        }
        return true;
    }

    virtual void async_run(async_callback<void> cb) override {
        log_info("authenticating");
        async_authenticate(
            use_this(std::move(cb), [](auto& this_, std::error_code ec, auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async_authenticate failed");
                log_info("authenticated");
                this_.start_autonomous_activities();
                cb({});
            }));
    }

    void start_autonomous_activities() {
        // What are App activities?
        // First, we need to find if we have a database for this account already. If so, then we
        // should find out if our local version (if any) is the same. If we don't have one, we
        // rebuild build the cache from stratch by first downloading all data and then processing
        // it.

        async_callback<void> local_sink_cb = [](std::error_code ec) {
            // ..
        };

        using namespace emailkit::imap_client;

        // list_t command returns a list of folders on mail server.
        // Typical Gmail mail server can look like:
        //     {inbox_path: ["INBOX"], flags: ["\\HasNoChildren"]}
        //     {inbox_path: ["[Gmail]"], flags: ["\\HasChildren", "\\Noselect"]}
        //     {inbox_path: ["[Gmail]", "Із зірочкою"], flags: ["\\Flagged", "\\HasNoChildren"]}
        //     {inbox_path: ["[Gmail]", "Важливо"], flags: ["\\HasNoChildren", "\\Important"]}
        //     {inbox_path: ["[Gmail]", "Кошик"], flags: ["\\HasNoChildren", "\\Trash"]}
        //     {inbox_path: ["[Gmail]", "Надіслані"], flags: ["\\HasNoChildren", "\\Sent"]}
        //     {inbox_path: ["[Gmail]", "Спам"], flags: ["\\HasNoChildren", "\\Junk"]}
        //     {inbox_path: ["[Gmail]", "Уся пошта"], flags: ["\\All", "\\HasNoChildren"]}
        //     {inbox_path: ["[Gmail]", "Чернетки"], flags: ["\\Drafts", "\\HasNoChildren"]}

        m_imap_client->async_execute_command(
            imap_commands::list_t{.reference_name = "", .mailbox_name = "*"},
            use_this(std::move(local_sink_cb), [](auto& this_, std::error_code ec,
                                                  types::list_response_t response,
                                                  auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async list command failed");

                log_info("executed list command:\n{}", fmt::join(response.inbox_list, "\n"));

                // The next step is to select some folder. We select INBOX first for this demo.
                // But sholud select all folders to build the cache.
                this_.m_imap_client->async_execute_command(
                    imap_commands::select_t{.mailbox_name = "INBOX"},
                    this_.use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                                     types::select_response_t response,
                                                     auto cb) mutable {
                        ASYNC_RETURN_ON_ERROR(ec, cb, "async select command failed");

                        log_info("selected INBOX folder (exists: {}, recents: {})", response.exists,
                                 response.recents);

                        // The nest step after selection is fetching emails or heders for
                        // emails.

                        // 			     IMAP was originally developed for the older
                        // [RFC-822] standard, and as a consequence several fetch items in IMAP
                        // incorporate "RFC822" in their name.  With the exception of
                        // RFC822.SIZE, there are more modern replacements; for example, the
                        // modern version of RFC822.HEADER is BODY.PEEK[HEADER].  In all cases,
                        // "RFC822" should be interpreted as a reference to the updated
                        // [RFC-2822] standard.

                        // Normally we should load emails by portions, say 10, 10, 10..
                        // If we hit a problem in some 10 with parsing, we can degrade to
                        // parsing individual emails and find a one which is not working and
                        // produce dummy entry instead so that in UI we can show something
                        // meaningful. The parser should be implemented in separate process with
                        // this design. This is going to be slow but we can probably optimize
                        // this part as part of separate release. We just need to have async
                        // interface to parser in the first place.

                        // If we have async inrerface to a module that parses IMAP
                        // and returns well known JSON, this can help to deal with parsing
                        // errors. To be effective we should also have a system that if some
                        // process crashes, we use spare process that is ready to accept data
                        // from us. In the same time, we can keep two more spare processes. On
                        // windows this is going to be slower. We can also detect hangs. With
                        // this, we can build reliable system for working with emails that can
                        // deal with things we cannot parse.
                        // The question remains whether we should download data from the server
                        // in the main process or not.
                        // There are drawback of this approach:
                        //  if we get helper process connected to the server, then it needs to
                        // be authenticated and own a socket. If it crashes because of parsing
                        // issue in individual email, we lose everything and need to reauth
                        // again.
                        // In the same time this creates even more isolation and encapsulation
                        // which enables nice testing by issuing command to proxe process and
                        // receive nice JSON's instead. This is like standalone cool piece of
                        // software which works as IMAP relay.
                        // Internally this piece of software can be orgianlized similarly to
                        // Git. Where we have a set of low level functions and we can create
                        // high level function using only low level. So whenever we have new use
                        // case, we scale horizontally by adding completely new code that uses
                        // well tested foundations of smaller functions.
                        // One more appraoch to this is to have two level: one process per email
                        // which owns a socket and set of processes that parse in separate
                        // process. This is kind of grotescue approach and more complicated
                        // but this way we can abstract out even more from things including
                        // ASIO.
                        // One of the questions with this design is how to communicate with
                        // external program. IO can be done be stdin/stdout using JSON as data
                        // format. But first lets prototype one simple idea: 1) Download and
                        // parse bodystructures for all documents so we can understand what are
                        // emails, what are their subjects, what are they attachments and what
                        // types they are, and all other useful metadata. With this function we
                        // can build local cache. Having this cache we next need to learn how to
                        // maintain this cache in actual state and update it incrementally.
                        // With this cache we can build some first UI that can display a list of
                        // emails, and build a tree. If this works, we can then think how to
                        // rework it into external process architecture.

                        this_.m_imap_client->async_execute_command(
                            imap_commands::fetch_t{
                                .sequence_set = imap_commands::raw_fetch_sequence_spec{"1:*"},
                                .items =
                                    imap_commands::fetch_items_vec_t{
                                        //					"BODY.PEEK[1.MIME]"}
                                        imap_commands::fetch_items::body_structure_t{},
                                        imap_commands::fetch_items::uid_t{},
                                        //                                            imap_commands::fetch_items::envelope_t{},
                                        imap_commands::fetch_items::rfc822_header_t{}}

                            },
                            this_.use_this(
                                std::move(cb), [](auto& this_, std::error_code ec,
                                                  types::fetch_response_t response, auto cb) {
                                    ASYNC_RETURN_ON_ERROR(ec, cb, "async fetch command failed");

                                    // Once we have bodystructure, we should parse it and create
                                    // a prototype of local DB.

                                    this_.process_bodystructure_response(response);

                                    cb({});
                                }));
                    }));
            }));

        // if (m_accounts.empty()) {
        //     // The following call is supposed to be capable to obtain some starting credentails.
        //     m_cbs.async_provide_credentials(use_this(
        //         std::move(local_sink_cb),
        //         [](auto& this_, std::error_code ec, std::vector<Account> accounts, auto cb) {
        //             // ..
        //         }));
        // }
    }

    void process_bodystructure_response(
        const emailkit::imap_client::types::fetch_response_t& response) {
        for (auto& [message_number, static_attributes] : response.message_data_items) {
            unsigned uid_value = 0;
            if (static_attributes.size() > 3) {
                log_warning(
                    "unexpected static attributes alongside of bodystructure, will be ignored ({})",
                    static_attributes.size());
            }
            for (auto& sattr : static_attributes) {
                if (std::holds_alternative<emailkit::imap_parser::wip::Body>(sattr)) {
                    auto& as_body = std::get<emailkit::imap_parser::wip::Body>(sattr);
                    traverse_body(as_body);
                    log_info("--------------------------------------------------------------");
                } else if (std::holds_alternative<emailkit::imap_parser::msg_attr_uid_t>(sattr)) {
                    auto& as_uid = std::get<emailkit::imap_parser::msg_attr_uid_t>(sattr);
                    uid_value = as_uid.value;
                    log_info("UID: {}", uid_value);
                } else if (std::holds_alternative<emailkit::imap_parser::Envelope>(sattr)) {
                    auto& as_envelope = std::get<emailkit::imap_parser::Envelope>(sattr);
                    // struct envelope_t {
                    //      std::optional<std::string> date_opt;

                    //     std::string subject;
                    //     std::string from;
                    //     std::string sender;
                    //     std::string reply_to;
                    //     std::string to;
                    //     std::string cc;
                    //     std::string bcc;
                    //     std::string in_reply_to;
                    //     std::string message_id;
                    // };
                } else {
                    log_warning("ignoring unexpected non-bodystructure stati attribute");
                    continue;
                }
            }
        }
        // ..
    }

    virtual void add_acount(Account acc) override {
        // ..
    }

    void async_authenticate(async_callback<void> cb) {
        // This should be hard-coded. Or loaded indirectly via special mode but still
        // hard-coded..= Because this is basically is part of our application.
        const auto app_creds = emailkit::google_auth_app_creds_t{
            .client_id = "303173870696-bsun94hmoseeumiat4iaa6dr752ta805.apps.googleusercontent.com",
            .client_secret = "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"};

        // https://developers.google.com/gmail/api/auth/scopes
        // We request control over mailbox and email address.
        const std::vector<std::string> scopes = {"https://mail.google.com",
                                                 "https://www.googleapis.com/auth/userinfo.email"};

        log_info("authentication in Google");

        m_google_auth->async_handle_auth(
            app_creds, scopes,
            [this, cb = std::move(cb)](std::error_code ec,
                                       emailkit::auth_data_t auth_data) mutable {
                if (ec) {
                    log_error("async_handle_auth failed: {}", ec);
                    return;
                }
                log_info("authenticated in Google");

                // now we can shutdown google auth server.

                // This means that user permitted all scopes we requested so we can proceed.
                // With permissions, google gave us all infromatiom we requested so the next
                // step would be to authenticate on google IMAP server.

                m_imap_client->async_connect(
                    "imap.gmail.com", "993",
                    [this, cb = std::move(cb), auth_data](std::error_code ec) mutable {
                        if (ec) {
                            log_error("failed connecting Gmail IMAP: {}", ec);
                            return;
                        }

                        // now we need to authenticate on imap server
                        m_imap_client->async_authenticate(
                            {.user_email = auth_data.user_email,
                             .oauth_token = auth_data.access_token},
                            [this, cb = std::move(cb)](
                                std::error_code ec,
                                emailkit::imap_client::auth_error_details_t err_details) mutable {
                                if (ec) {
                                    log_error("imap auth failed: {}", ec);
                                    // TODO: how we are supposed to handle this error?
                                    // I gues we shoud raise special error for this case and
                                    // depdening on why auth failed either retry with same creds
                                    // (if there is no Internet), or go to authentication. Or,
                                    // we should somehow allow user to start over.
                                    cb(ec);
                                    return;
                                }

                                log_info("authenticated on IMAP server");
                                // TODO: consider having some kind of connection info state
                                // which would indicate information about all current
                                // connections.

                                cb({});
                            });
                    });
            });
    }

   private:
    asio::io_context& m_ctx;
    std::shared_ptr<emailkit::imap_client::imap_client_t> m_imap_client;
    std::shared_ptr<emailkit::google_auth_t> m_google_auth;
    EmailAppCoreCallbacks& m_cbs;
    std::vector<Account> m_accounts;
};

std::shared_ptr<EmailAppCore> make_emailapp_core(asio::io_context& ctx,
                                                 EmailAppCoreCallbacks& cbs) {
    auto app_instance = std::make_shared<EmailAppCoreImpl>(ctx, cbs);
    if (!app_instance->initialize()) {
        return nullptr;
    }
    return app_instance;
}

}  // namespace emailapp::core
