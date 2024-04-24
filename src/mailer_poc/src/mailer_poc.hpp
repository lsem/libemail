#pragma once

#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include "mailer_ui_state.hpp"

namespace mailer {

enum class ApplicationState {
    // Application is being initialized and not ready to accept requesets.
    not_ready,

    // This state means that application has no valid pair of credentials which for Google means no
    // token or for regular IMAP session means some password.
    // Application requires to invoke login() method.
    login_required,

    // This state is set after async_request_gmail_auth is successfuly finished. This means that web
    // server or other auth method
    // is ready and we are waiting for the input from the user.
    waiting_auth,

    connecting_to_test,
    connected_to_test,

    ready_to_connect,

    // This means that appplication has credentials (either valid or not) and application is trying
    // to establish imap connection.
    // If there is no internet condition and we have some credentials application will be in this
    // state even though there could be back-offs between attempts. If application encounters errors
    // and have doubts in credentials validity, application may transition to
    // ApplicationState::login_required state.
    imap_authenticating,

    // This state means that we have established IMAP connection with the server. In future this may
    // be split into more states like synchronizing, idle, etc..
    imap_established,
};

inline std::string to_string(ApplicationState s) {
    switch (s) {
        case ApplicationState::not_ready:
            return "not_ready";
        case ApplicationState::login_required:
            return "login_required";
        case ApplicationState::waiting_auth:
            return "waiting_auth";
        case ApplicationState::connecting_to_test:
            return "connecting_to_test";
        case ApplicationState::connected_to_test:
            return "connected_to_test";
        case ApplicationState::ready_to_connect:
            return "ready_to_connect";
        case ApplicationState::imap_authenticating:
            return "imap_authenticating";
        case ApplicationState::imap_established:
            return "imap_established";
        default:
            return "<ApplicationState:unknown>";
    }
}

inline std::ostream& operator<<(std::ostream& os, ApplicationState s) {
    os << to_string(s);
    return os;
}

struct IMAPConnectionCreds {
    std::string host;  // imap.gmail.com
    int port{};        // 993
    types::EmailAddress email_address;
    std::string access_token;

    // TODO: this class should be reworked to type erased value type instead because now it is
    // actually only for gmail.
};

inline std::ostream& operator<<(std::ostream& os, const IMAPConnectionCreds& creds) {
    os << "IMAPConnectionCreds(host: " << creds.host << ", port: " << creds.port
       << ", email_address: " << creds.email_address << ", access_token: " << creds.access_token
       << ")";
    return os;
}

struct MailerPOCCallbacks {
   protected:
    ~MailerPOCCallbacks() = default;

   public:
    // Auth server is ready on URI. We can improve overall handling by first requesting
    // reauth so the client can prepare UI accordingly, then resolve callback and only then
    // have server started.
    virtual void state_changed(ApplicationState s) = 0;
    virtual void auth_initiated(std::string uri) = 0;
    virtual void auth_done(std::error_code, IMAPConnectionCreds) = 0;
    virtual void tree_about_to_change() = 0;
    virtual void tree_model_changed() = 0;

    // This function must be execute fb in the UI thread. This supposedly is the only place
    // where it is safe to update the model.
    virtual void update_state(std::function<void()> fn) = 0;
};

// It seems like we need to have a notion of application state and state subscription.
// So it seems like we need new callback, application state changed:
//     void on_state_changed()
//

struct AuthStartDetails {
    std::string uri;
};

class MailerPOC {
   public:
    virtual ~MailerPOC() = default;
    virtual void start_working_thread() = 0;
    virtual void stop_working_thread() = 0;
    virtual void async_run(async_callback<void> cb) = 0;
    virtual void set_callbacks_if(MailerPOCCallbacks* callbacks) = 0;
    virtual void visit_model_locked(std::function<void(const mailer::MailerUIState&)> cb) = 0;
    virtual void selected_folder_changed(TreeNode* selected_node) = 0;
    virtual TreeNode* make_folder(TreeNode* parent, string folder_name) = 0;
    virtual void move_items(std::vector<TreeNode*> source_nodes,
                            TreeNode* dest,
                            optional<size_t> dest_row) = 0;
    virtual MailerUIState* get_ui_model() = 0;

    // Prepare GMAIL authentication. Supposed to be called as a result of making action "Login Via
    // Google" in any form. In current implementation it brings up web server.
    // Once this is ready, the GUI should expect that callback about auth completion
    // (auth_done(std::error_code)) may come anytime, after this the state of the application may
    // also change just after IMAP connection.
    // TODO:But if this really what we need? We may want to test
    // connection before fully switching into a state and accept the creds as valid. Need to think
    // about it.
    // The question:
    // How we are supposed to stop the server? Should we have a call like cancel/abort?
    // I guess so.
    virtual void async_request_gmail_auth(async_callback<AuthStartDetails> cb) = 0;

    // Supposed to be caleld after async_request_gmail_auth.
    virtual void async_wait_auth_done(async_callback<IMAPConnectionCreds> cb) = 0;
    // Antother appraoch is to let the GUI first test if connection works. E.g. by issung
    // async_test_connection(aysnc_callabck<void>). And only then call proceed_with_creds().

    // Adds credentials into a system. Fromt this point the system will be running autoconnect loop
    // trying to establish connection with IMAP server. See also: async_test_creds method.
    virtual void async_accept_creds(IMAPConnectionCreds, async_callback<void>) = 0;

    // Attempts to connect with specified creds. Creds have all necessarry information inside
    // effectively encapsulating credentials and server address.
    virtual void async_test_creds(IMAPConnectionCreds creds, async_callback<void> cb) = 0;

    virtual ApplicationState get_state() = 0;
};

std::shared_ptr<MailerPOC> make_mailer_poc();
}  // namespace mailer
