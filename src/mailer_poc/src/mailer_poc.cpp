#include "mailer_poc.hpp"
#include <emailkit/google_auth.hpp>
#include <emailkit/imap_client.hpp>

namespace mailer {

class MailerPOC_impl : public MailerPOC, EnableUseThis<MailerPOC_impl> {
   public:
    explicit MailerPOC_impl(asio::io_context& ctx) : m_ctx(ctx) {}

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

   public:
    void async_run(async_callback<void> cb) override {
        log_info("authenticating");
        async_authenticate(
            use_this(std::move(cb), [](auto& this_, std::error_code ec, auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async_authenticate failed");
                log_info("authenticated");
                //                this_.start_autonomous_activities();
                cb({});
            }));
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
    std::shared_ptr<emailkit::google_auth_t> m_google_auth;
    std::shared_ptr<emailkit::imap_client::imap_client_t> m_imap_client;
};

std::shared_ptr<MailerPOC> make_mailer_poc(asio::io_context& ctx) {
    auto inst = std::make_shared<MailerPOC_impl>(ctx);
    if (!inst) {
        log_error("failed creating instance of the app");
        return nullptr;
    }
    return inst;
}

}  // namespace mailer
