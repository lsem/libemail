#include "mailer_poc.hpp"
#include <emailkit/emailkit.hpp>
#include <emailkit/google_auth.hpp>
#include <emailkit/imap_client.hpp>
#include <emailkit/utils.hpp>

#include <fmt/ranges.h>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>

#include "mailer_ui_state.hpp"

#include "user_tree.hpp"

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
constexpr auto FOLDERS_PATH = "folders.json";

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

class MailerPOC_impl : public MailerPOC,
                       public EnableUseThis<MailerPOC_impl>,
                       public MailerUIStateParent {
   public:
    explicit MailerPOC_impl(asio::io_context& ctx) : m_ctx(ctx), m_callbacks(nullptr) {}
    ~MailerPOC_impl() { emailkit::finalize(); }

   public:  // MailerUIStateParent
    void on_tree_changed() override { on_tree_structure_changed(); }

   public:
    bool initialize() {
        m_ui_state.attach_parent(this);

        m_imap_client = emailkit::imap_client::make_imap_client(m_ctx);
        if (!m_imap_client) {
            log_error("failed creating imap client");
            return false;
        }

        // TODO: fix this usage.
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

                // TODO: Shouldn't we synchronize with UI at this point? To make sure that UI is up
                // and running so we can use UI-callbacks?

                this_.initialize_tree();

                this_.run_background_activities();
                cb({});
            }));
    }

    void set_callbacks_if(MailerPOCCallbacks* callbacks) override { m_callbacks = callbacks; }

    void on_tree_structure_changed() {
        auto root_or_err = uitree_to_user_tree(get_ui_model()->tree_root());
        if (!root_or_err) {
            // TODO: more error reporting?
            log_error("failed converting uitree to user tree: {}", root_or_err.error());
            return;
        }
        if (auto void_or_err = user_tree::save_tree_to_file(*root_or_err, FOLDERS_PATH);
            !void_or_err) {
            log_error("failed saving folders into a file: {}", void_or_err.error());
        }
    }

    void user_tree_to_ui_tree_it(const user_tree::Node& src_node, TreeNode* dest_node) {
        // TODO: dest_node myst be precreated so we just need to turn dest_node into src_node
        dest_node->label = src_node.label;
        dest_node->flags = src_node.flags;
        dest_node->contact_groups =
            set<set<string>>(src_node.contact_groups.begin(), src_node.contact_groups.end());
        assert(dest_node->is_folder_node());
        for (auto& c : src_node.children) {
            auto child_dest_node = new TreeNode{};
            child_dest_node->parent = dest_node;
            dest_node->children.emplace_back(child_dest_node);
            user_tree_to_ui_tree_it(*c, child_dest_node);
        }
    }

    void save_tree_it(const TreeNode* src_node, user_tree::Node& dest_node) {
        assert(src_node);
        // TODO: what if root is not a folder at all? The caller should guarantee this or otherwise
        // we will have invalid node (we cna return bool I guess).
        if (src_node->is_folder_node()) {
            dest_node.label = src_node->label;
            dest_node.flags = src_node->flags;
            dest_node.contact_groups = vector<set<string>>(src_node->contact_groups.begin(),
                                                           src_node->contact_groups.end());

            for (auto& c : src_node->children) {
                if (c->is_folder_node()) {
                    auto child_dest_node = std::make_unique<user_tree::Node>();
                    save_tree_it(c, *child_dest_node);
                    dest_node.children.emplace_back(std::move(child_dest_node));
                }
            }
        }

        // TODO: seems like we need to somehow restore cache after we constructed the tree.
        // Normally, when folders are crated, the cache is updated immidiately after it.
        // The same with folder move.
    }

    expected<user_tree::Node> uitree_to_user_tree(const TreeNode* src_node) {
        if (!src_node->is_folder_node()) {
            log_error("attempt to save starting from non-folder root");
            return unexpected(make_error_code(std::errc::io_error));
        }

        user_tree::Node root_dest;
        save_tree_it(src_node, root_dest);

        return root_dest;
    }

    void initialize_tree() {
        auto root_or_err = user_tree::load_tree_from_file(FOLDERS_PATH);
        if (!root_or_err) {
            log_warning("fialed loading tree: {}", root_or_err.error());
            return;
        }
        log_info("loaded initial tree:");
        user_tree::print_tree(*root_or_err);
        user_tree_to_ui_tree_it(*root_or_err, get_ui_model()->tree_root());
        get_ui_model()->rebuild_caches_after_tree_reconstruction();
    }

    void visit_model_locked(std::function<void(const mailer::MailerUIState&)> cb) override {
        // TODO: detect slow visits!
        std::scoped_lock<std::mutex> locked(m_ui_state_mutex);
        cb(m_ui_state);
    }

    void selected_folder_changed(TreeNode* selected_node) override {
        // if (!selected_node->ref) {
        //     log_debug("selected folder changed, here is a list of threads in given folder");
        //     for (auto& c : selected_node->children) {
        //         if (c->ref) {
        //             log_debug("{}", c->ref->label);
        //         }
        //     }
        // }
    }

    TreeNode* make_folder__dont_notify(TreeNode* parent, string folder_name) {
        return m_ui_state.make_folder(parent, folder_name);
    }

    TreeNode* make_folder(TreeNode* parent, string folder_name) override {
        auto result = make_folder__dont_notify(parent, folder_name);
        on_tree_structure_changed();
        return result;
    }

    void move_items(std::vector<mailer::TreeNode*> source_nodes,
                    mailer::TreeNode* dest,
                    optional<size_t> dest_row) override {
        // TODO: theoretically we should be able to see that we are on the same GUI thread and omit
        // update through dispatch and call directly. Theoretically this passing nodes does not play
        // nicely with concrrency since at the moment we handled
        m_callbacks->tree_about_to_change();
        log_info("tree before: {}", render_tree(m_ui_state));
        m_ui_state.move_items(source_nodes, dest, dest_row);
        log_info("tree after: {}", render_tree(m_ui_state));
        m_callbacks->tree_model_changed();

        on_tree_structure_changed();

        // if (std::any_of(source_nodes.begin(), source_nodes.end(),
        //                 [](auto& x) { return x->is_folder_node(); })) {
        //     on_tree_structure_changed();
        // }
    }

    MailerUIState* get_ui_model() override { return &m_ui_state; }

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
            use_this(std::move(cb), [](auto& this_, std::error_code ec,
                                       emailkit::auth_data_t auth_data, auto cb) mutable {
                if (ec) {
                    log_error("async_handle_auth failed: {}", ec);
                    return;
                }
                log_info("authenticated in Google");

                this_.m_ui_state.set_own_address(auth_data.user_email);

                // now we can shutdown google auth server.

                // This means that user permitted all scopes we requested so we can proceed.
                // With permissions, google gave us all infromatiom we requested so the next
                // step would be to authenticate on google IMAP server.

                this_.m_imap_client->async_connect(
                    "imap.gmail.com", "993",
                    this_.use_this(std::move(cb), [auth_data](auto& this_, std::error_code ec,
                                                              auto cb) mutable {
                        if (ec) {
                            log_error("failed connecting Gmail IMAP: {}", ec);
                            return;
                        }

                        // now we need to authenticate on imap server
                        this_.m_imap_client->async_authenticate(
                            {.user_email = auth_data.user_email,
                             .oauth_token = auth_data.access_token},
                            this_.use_this(
                                std::move(cb),
                                [](auto& this_, std::error_code ec,
                                   emailkit::imap_client::auth_error_details_t err_details,
                                   auto cb) mutable {
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
                                }));
                    }));
            }));
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

        if (is_noselect_box(e) || e.inbox_path != vector<string>({"INBOX"})) {
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

                // this_.m_imap_client->async_execute_command(
                //     emailkit::imap_client::imap_commands::fetch_t{
                //         .sequence_set =
                //             emailkit::imap_client::imap_commands::raw_fetch_sequence_spec{
                //                 fmt::format("{}:{}", 55, 55)},
                //         .items =
                //             emailkit::imap_client::imap_commands::fetch_items_vec_t{
                //                 emailkit::imap_client::imap_commands::fetch_items::uid_t{},
                //                 // emailkit::imap_client::imap_commands::fetch_items::
                //                 //     body_structure_t{},
                //                 emailkit::imap_client::imap_commands::fetch_items::
                //                     rfc822_header_t{}}},
                //     this_.use_this(std::move(cb),
                //                    [](auto& this_, std::error_code ec, auto response, auto cb) {
                //                        if (!ec) {
                //                            log_warning("something strange!");
                //                        }
                //                        cb({});
                //                    }));
                this_.async_download_emails_for_mailbox(
                    result.raw_response.exists, std::move(mailbox_path_parts),
                    this_.use_this(std::move(cb), [list_entries = std::move(list_entries)](
                                                      auto& this_, std::error_code ec, auto cb) {
                        ASYNC_RETURN_ON_ERROR(ec, cb,
                                              "async downlaod emails for mailbox failed: {}");
                        this_.async_download_all_mailboxes_it(std::move(list_entries),
                                                              std::move(cb));
                    }));
            }));
    }

    void async_download_emails_for_mailbox_sequential_it(
        int from,
        int to,
        std::vector<emailkit::types::MailboxEmail> acc_vec,
        async_callback<std::vector<emailkit::types::MailboxEmail>> cb) {
        if (from > to) {
            log_debug("from > to");
            cb({}, std::move(acc_vec));
            return;
        }
        m_imap_client->async_list_items(
            from, from,
            use_this(std::move(cb),
                     [from, to, acc_vec = std::move(acc_vec)](
                         auto& this_, std::error_code ec,
                         std::variant<string, vector<emailkit::types::MailboxEmail>> items_or_text,
                         auto cb) mutable {
                         if (ec) {
                             log_error("failed downloading email with uid {}: {}", from, ec);
                             emailkit::types::MailboxEmail email;
                             email.message_uid = from;
                             email.is_valid = false;
                             email.subject = fmt::format("<FAILED TO DOWNLOAD EMAIL #{}", from);
                             acc_vec.emplace_back(std::move(email));

                             if (std::holds_alternative<string>(items_or_text)) {
                                 auto failed_raw_imap = std::get<string>(items_or_text);
                                 std::string file_name = fmt::format(
                                     "failed-{}-{}", from, std::hash<string>{}(failed_raw_imap));
                                 log_warning("writing failed email body into {}", file_name);
                                 std::ofstream f(file_name,
                                                 std::ios_base::out | std::ios_base::binary);
                                 if (f.good()) {
                                     f << failed_raw_imap;
                                     f.close();
                                 } else {
                                     log_error("failed writing file with bad IMAP into disk");
                                 }
                             }
                         } else {
                             assert(std::holds_alternative<vector<emailkit::types::MailboxEmail>>(
                                 items_or_text));
                             for (auto& x :
                                  std::get<vector<emailkit::types::MailboxEmail>>(items_or_text)) {
                                 acc_vec.emplace_back(std::move(x));
                             }
                         }
                         this_.async_download_emails_for_mailbox_sequential_it(
                             from + 1, to, std::move(acc_vec), std::move(cb));
                     }));
    }

    void async_download_emails_for_mailbox_sequential(
        int from,
        int to,
        async_callback<std::vector<emailkit::types::MailboxEmail>> cb) {
        async_download_emails_for_mailbox_sequential_it(from, to, {}, std::move(cb));
    }

    // |----------|------------|--------------|-----|

    // TODO: what if new email is received on the server while we are downloading folder?
    void async_download_emails_for_mailbox_it(int from,
                                              int N,
                                              std::vector<std::string> folder_path,
                                              async_callback<void> cb) {
        if (from > N) {
            log_info("finished at from={}, N={}", from, N);
            cb({});
            return;
        }
        const int BATCH_SIZE = 50;
        int to = from + std::min(N - from, BATCH_SIZE);

        log_info("downloading next batch, from: {}, to: {}", from, to);

        m_imap_client->async_list_items(
            from, to,
            use_this(std::move(cb), [from, to, N, folder_path = std::move(folder_path)](
                                        auto& this_, std::error_code ec,
                                        std::variant<string,
                                                     std::vector<emailkit::types::MailboxEmail>>
                                            items_or_text,
                                        auto cb) mutable {
                if (ec) {
                    log_error(
                        "async list items failed on batch {}:{}, falling back to "
                        "sequential fetch: {}",
                        from, to, ec);
                    // Fallback to sequential download which will create dummy emails for ones that
                    // we failed to download.
                    this_.async_download_emails_for_mailbox_sequential(
                        from, to,
                        this_.use_this(
                            std::move(cb), [from, to, folder_path = std::move(folder_path)](
                                               auto& this_, std::error_code ec,
                                               std::vector<emailkit::types::MailboxEmail> emails,
                                               auto cb) mutable {
                                if (ec) {
                                    log_error(
                                        "sequential download (fallback) failed to download a batch "
                                        "{}{}: {} ",
                                        from, to, ec);
                                    cb(ec);
                                    return;
                                }
                                // TODO: note, this is async processing.
                                this_.process_email_folder(folder_path, std::move(emails));
                                cb({});
                            }));
                    return;
                }

                // TODO: this should probably be named async_ since this is done in UI
                // thread.
                this_.process_email_folder(
                    folder_path,
                    std::move(std::get<std::vector<emailkit::types::MailboxEmail>>(items_or_text)));
                this_.async_download_emails_for_mailbox_it(to + 1, N, std::move(folder_path),
                                                           std::move(cb));
            }));
    }

    // Downlaods all emails from currenty selected inbox.
    void async_download_emails_for_mailbox(int mailbox_size,
                                           std::vector<std::string> folder_path,
                                           async_callback<void> cb) {
        // TODO: in real world program this should not be unbound list but some fixed bucket

        async_download_emails_for_mailbox_it(1, mailbox_size, std::move(folder_path),
                                             std::move(cb));
    }

    void process_email_folder(vector<string> folder_path,
                              const vector<emailkit::types::MailboxEmail> emails_meta) {
        assert(m_callbacks);

        // TODO: we need more synchronization here. This should probably have continuation
        // instead of just scheduling updated into UI thread for each email. Another idea is
        // that we can. One intellegent approach is to have a timed update. E.g. having UI
        // updated no more then 50ms in 1s or something like this.

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

    MailerUIState m_ui_state{""};
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
