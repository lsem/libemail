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

	// TODO: here should be more intellectuay way for selecting this IP/port combination.
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
        async_callback<void> local_sink_cb = [](std::error_code ec) {
            if (ec) {
                log_error("cannot start autonomous activities, error occurred: {}", ec);
            } else {
                log_info("autonomous activities have been started");
            }
        };

        using namespace emailkit::imap_client;

        m_imap_client->async_list_mailboxes(use_this(
            std::move(local_sink_cb),
            [](auto& this_, std::error_code ec, imap_client_t::ListMailboxesResult result,
               auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async list mailboxes failed");
                log_info("executed list_mailboxes:\n{}",
                         fmt::join(result.raw_response.inbox_list, "\n"));
		
                // TODO: here we do select of hardcoded name which means 'default on this serverfor
                // this user' but we should probably dig more into this.
                this_.m_imap_client->async_select_mailbox(
                    "INBOX",
                    this_.use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                                     imap_client_t::SelectMailboxResult result,
                                                     auto cb) mutable {
                        ASYNC_RETURN_ON_ERROR(ec, cb, "async select mailbox failed");
                        log_info("selected INBOX folder (exists: {}, recents: {})", result.exists,
                                 result.recents);

                        this_.m_imap_client->async_list_items(
                            1, std::nullopt,
                            this_.use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                                             auto cb) mutable {
                                ASYNC_RETURN_ON_ERROR(ec, cb, "async list items failed");
                                cb({});
                            }));
                    }));
            }));
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
