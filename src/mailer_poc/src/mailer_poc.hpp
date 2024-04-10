#pragma once

#include "mailer_ui_state.hpp"

namespace mailer {

struct MailerPOCCallbacks {
   protected:
    ~MailerPOCCallbacks() = default;

   public:
    // Auth server is ready on URI. We can improve overall handling by first requesting reauth so
    // the client can prepare UI accordingly, then resolve callback and only then have server
    // started.
    virtual void auth_initiated(std::string uri) = 0;
    virtual void auth_done(std::error_code) = 0;
    virtual void tree_about_to_change() = 0;
    virtual void tree_model_changed() = 0;

    // This function must be execute fb in the UI thread. This supposedly is the only place where it
    // is safe to update the model.
    virtual void update_state(std::function<void()> fn) = 0;
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
};

std::shared_ptr<MailerPOC> make_mailer_poc();
}  // namespace mailer
