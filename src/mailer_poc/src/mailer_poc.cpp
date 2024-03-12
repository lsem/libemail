#include "mailer_poc.hpp"
#include <emailkit/emailkit.hpp>
#include <emailkit/google_auth.hpp>
#include <emailkit/imap_client.hpp>
#include <emailkit/utils.hpp>

#include <fmt/ranges.h>
#include <mutex>
#include <set>
#include <sstream>

#include "mailer_ui_state.hpp"

namespace mailer {
using namespace emailkit;
using emailkit::imap_client::types::list_response_entry_t;
using emailkit::types::EmailAddress;
using emailkit::types::MessageID;

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace {
string render_tree(const mailer::MailerUIState& ui_state) {
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
            ss << fmt::format("{}{} (emails: {}{})\n", indent_str,
                              (ref.label.empty() ? "<No-Subject>" : ref.label), ref.emails_count,
                              ref.attachments_count > 0
                                  ? fmt::format(", attachments: {}", ref.attachments_count)
                                  : "");
        });

    return ss.str();
}

}  // namespace

class TheTree {
    using UID = std::string;

   public:
    void add_group(std::string group_name) { m_groups_ids[group_name] = generate_random_id(); }
    void add_address(const EmailAddress& a) { m_address_ids[a] = generate_random_id(); }

    std::string generate_random_id() {
        std::string s;
        // TODO: use UID
        // https://github.com/libstud/libstud-uuid
        for (int i = 0; i < 12; ++i) {
            s += rand() % ('A' - 'Z') + 'A';
        }
        return s;
    }

   private:
    map<EmailAddress, UID> m_address_ids;
    map<string, UID> m_groups_ids;
    map<UID, UID> m_relations;
};

struct MailIDFilter {
    inline static map<string, string> m_data;
    inline static int s_i = 0;

    MessageID process(MessageID id) {
        if (m_data.find(id) != m_data.end()) {
            return m_data[id];
        } else {
            log_debug("assigning {} to id {}", s_i, id);
            auto x = s_i++;
            m_data[id] = std::to_string(x);
            return std::to_string(x);
        }
    }
};

class MailerPOC_impl : public MailerPOC, public EnableUseThis<MailerPOC_impl> {
   public:
    explicit MailerPOC_impl(asio::io_context& ctx) : m_ctx(ctx), m_callbacks(nullptr) {}
    ~MailerPOC_impl() { emailkit::finalize(); }

    bool initialize() {
        m_imap_client = emailkit::imap_client::make_imap_client(m_ctx);
        if (!m_imap_client) {
            log_error("failed creating imap client");
            return false;
        }

        m_google_auth =
            emailkit::make_google_auth(m_ctx, "127.0.0.1", "8089", [this](std::string uri) {
                log_info("calling callbacks");
                assert(m_callbacks);
                m_callbacks->auth_initiated(uri);
            });
        if (!m_google_auth) {
            log_error("failed creating google auth");
            return false;
        }

        if (!emailkit::initialize()) {
            log_error("failed initializing emailkit");
            return false;
        }
        return true;
    }

   public:
    void async_run(async_callback<void> cb) override {
        log_info("authenticating");
        async_authenticate(
            use_this(std::move(cb), [](auto& this_, std::error_code ec, auto cb) mutable {
                if (ec) {
                    log_error("async_authenticate failed: {}", ec);
                    assert(this_.m_callbacks);
                    this_.m_callbacks->auth_done(ec);
                    cb(ec);
                    return;
                }

                log_info("authenticated");
                assert(this_.m_callbacks);
                this_.m_callbacks->auth_done({});
                this_.run_background_activities();
                cb({});
            }));
    }
    void set_callbacks_if(MailerPOCCallbacks* callbacks) override { m_callbacks = callbacks; }

    void visit_model_locked(std::function<void(const mailer::MailerUIState&)> cb) override {
        // TODO: detect slow visits!
        std::scoped_lock<std::mutex> locked(m_ui_state_mutex);
        cb(m_ui_state);
    }

    virtual void selected_folder_changed(MailerUIState::TreeNode* selected_node) override {
        if (!selected_node->ref) {
            log_debug("selected folder changed, here is a list of threads in given folder");
            for (auto& c : selected_node->children) {
                if (c->ref) {
                    log_debug("{}", c->ref->label);
                }
            }
        }
    }

    MailerUIState* get_ui_model() { return &m_ui_state; }

    void run_background_activities() {
        async_callback<void> cb = [](std::error_code ec) {

        };

        log_info("downloading all emails from all mailboxes..");
        async_download_all_emails(std::move(cb));
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

    struct Email {
        std::string subject;
        std::string timestamp;
    };

    void async_download_all_emails(async_callback<void> cb) {
        using namespace emailkit;
        using namespace emailkit::imap_client;

        m_imap_client->async_list_mailboxes(
            use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                       imap_client_t::ListMailboxesResult result, auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async list mailboxes failed");
                log_info("executed list_mailboxes:\n{}",
                         fmt::join(result.raw_response.inbox_list, "\n"));

                this_.async_download_all_mailboxes(result.raw_response.inbox_list, std::move(cb));
            }));
    }

    void async_download_all_mailboxes(std::vector<list_response_entry_t> list_entries,
                                      async_callback<void> cb) {
        std::reverse(list_entries.begin(), list_entries.end());
        async_download_all_mailboxes_it(std::move(list_entries), std::move(cb));
    }

    void async_download_all_mailboxes_it(std::vector<list_response_entry_t> list_entries,
                                         async_callback<void> cb) {
        if (list_entries.empty()) {
            // done
            cb({});
            return;
        }

        auto e = std::move(list_entries.back());
        list_entries.pop_back();

        std::string mailbox_path =
            fmt::format("{}", fmt::join(e.inbox_path, e.hierarchy_delimiter));

        log_info("selecting mailbox '{}'", mailbox_path);

        // TODO: this should be done at imap client level involving corresponding RFC.
        // The raw command should not interpret this fields. But non-raw command should be aware
        // of RFC and can parse this into enum flags.
        auto is_noselect_box = [](auto& x) {
            return std::find(x.flags.begin(), x.flags.end(), "\\Noselect") != x.flags.end();
        };

        if (is_noselect_box(e)) {
            log_info("skipping noselect folder '{}'", mailbox_path);
            async_download_all_mailboxes_it(std::move(list_entries), std::move(cb));
            return;
        }

        // The IMAP protocol is inherently stateful. One should select a mailbox and do fetches
        // on it. So download all the emails we need to select each folder and list one by one.
        log_info("selecting mailbox: {}", e.mailbox_raw);
        m_imap_client->async_select_mailbox(
            e.mailbox_raw,
            use_this(std::move(cb), [mailbox_path_parts = e.inbox_path, mailbox_path,
                                     list_entries = std::move(list_entries)](
                                        auto& this_, std::error_code ec,
                                        imap_client::imap_client_t::SelectMailboxResult result,
                                        auto cb) mutable {
                ASYNC_RETURN_ON_ERROR(ec, cb, "async select mailbox failed");
                log_info("selected {} folder (exists: {}, recents: {})", mailbox_path,
                         result.raw_response.exists, result.raw_response.recents);

                log_info("downloading emails on selected folder");
                this_.async_download_emails_for_mailbox(
                    std::move(mailbox_path_parts),
                    this_.use_this(std::move(cb), [list_entries = std::move(list_entries)](
                                                      auto& this_, std::error_code ec, auto cb) {
                        ASYNC_RETURN_ON_ERROR(ec, cb,
                                              "async downlaod emails for mailbox failed: {}");
                        this_.async_download_all_mailboxes_it(std::move(list_entries),
                                                              std::move(cb));
                    }));
            }));
    }

    // Downlaods all emails from currenty selected inbox.
    void async_download_emails_for_mailbox(std::vector<std::string> folder_path,
                                           async_callback<void> cb) {
        // TODO: in real world program this should not be unbound list but some fixed bucket
        // size.
        m_imap_client->async_list_items(
            1, std::nullopt,
            use_this(std::move(cb),
                     [folder_path = std::move(folder_path)](
                         auto& this_, std::error_code ec,
                         std::vector<emailkit::types::MailboxEmail> items, auto cb) mutable {
                         ASYNC_RETURN_ON_ERROR(ec, cb, "async list items failed");
                         this_.process_email_folder(folder_path, std::move(items));
                         cb({});
                     }));
    }

    void process_email_folder(vector<string> folder_path,
                              const vector<emailkit::types::MailboxEmail> emails_meta) {
        assert(m_callbacks);

        // TODO: we need more synchronization here. This should probably have continuation instead
        // of just scheduling updated into UI thread for each email. Another idea is that we can.
        // One intellegent approach is to have a timed update. E.g. having UI updated no more then
        // 50ms in 1s or something like this.

        m_callbacks->update_state([emails_meta, this] {
            m_callbacks->tree_about_to_change();

            for (auto& m : emails_meta) {
                m_ui_state.process_email(m);
            }
            log_info("\n{}", render_tree(m_ui_state));

            m_callbacks->tree_model_changed();
        });
    }

   private:
    asio::io_context& m_ctx;

    MailerPOCCallbacks* m_callbacks;
    std::shared_ptr<emailkit::google_auth_t> m_google_auth;
    std::shared_ptr<emailkit::imap_client::imap_client_t> m_imap_client;

    MailerUIState m_ui_state{"liubomyr.semkiv.test@gmail.com"};  // TODO: we have this during login!
    std::mutex m_ui_state_mutex;

    MailIDFilter m_idfilter;
};

std::shared_ptr<MailerPOC> make_mailer_poc(asio::io_context& ctx) {
    auto inst = std::make_shared<MailerPOC_impl>(ctx);
    if (!inst->initialize()) {
        log_error("failed creating instance of the app");
        return nullptr;
    }
    return inst;
}

// TODO NEXT FEATURES:
// 1. Emails list (thread) representation for UI and API for working with them.
// 2. Receiving new emails as they arrive and rebuild UI accordingly.
// 3. Folders for contacts.
// 4. Sending emails via SMTP and saving custom contact book.
// 5. Caching, UIDs, invalidating, etc..
// 6. Custom standards review, capabilities, useful extensions, etc..

}  // namespace mailer
