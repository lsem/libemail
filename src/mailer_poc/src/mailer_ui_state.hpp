#pragma once
#include <emailkit/emailkit.hpp>
#include <emailkit/google_auth.hpp>
#include <emailkit/imap_client.hpp>
#include <emailkit/utils.hpp>

#include <fmt/ranges.h>
#include <set>

namespace mailer {
using namespace emailkit;
using emailkit::imap_client::types::list_response_entry_t;
using emailkit::types::EmailAddress;
using emailkit::types::MessageID;

// A class responsible for processing emails. When email arrives we execute this function to add it
// to the UI. After processing of it, the model of the UI may be changed so one can rerender it.
class MailerUIState {
   public:
    explicit MailerUIState(types::EmailAddress own_address)
        : m_own_address(std::move(own_address)) {}

    void process_email(const types::MailboxEmail& email) {
        // When emails are added the only we do is that we create folders or remove folders.
        // That's all.
        if (!email.message_id.has_value()) {
            log_error("Message without message ID is not suppoered: {}", to_json(email));
            return;
        }

        if (email.from.empty()) {
            log_error("empty FROM is not supported by UI, rejecting email: {}", to_json(email));
            // TODO: consider adding blank or special folder like [BROKEN EMAILS].
            return;
        }

        if (email.from.size() > 1) {
            log_warning(
                "multiple FROM address not supported by UI, ignoring the rest and taking just "
                "first one: {}",
                to_json(email));
        }

        auto& from = email.from[0];

        static vector<types::MessageID> no_refs;
        auto& references = email.references.has_value() ? email.references.value() : no_refs;

        // References contains a complete list for given thread (this is my hypothesis) so that we
        // now that these are related. But TO and CC/BCC field contains who actually received this
        // particular message.

        // By inspecting references we theoretically can find messages we already have in our
        // system/UI and exclude them. So we should maintain an index from MessageID to path in a
        // tree.p

        // Index
        m_message_id_to_email_index[email.message_id.value()] = email;
        log_debug("email with ID '{}' added to the index", email.message_id.value());

        // Try to find a reference to existing conversation

        optional<types::MessageID> thread_id_opt;
        TreeNode* thread_id_node = nullptr;
        for (auto& mid : references) {
            if (auto it = m_thread_id_to_tree_index.find(mid);
                it != m_thread_id_to_tree_index.end()) {
                thread_id_opt = mid;
                thread_id_node = it->second;
            }
        }

        if (thread_id_opt) {
            log_debug("found thread id {}, node: {}", *thread_id_opt,
                      static_cast<void*>(thread_id_node));
        }

        // References are not empty, so I suppose this is a reply to already established
        // conversation.

        // So if we find emails we should now move them into new directory.

        // The task here is to find proper folder which is created from participants list.
        // Participants list includes receiver and all destinations from entire discussion.
        // If resulting participants list consits only from one sender/receiver and one
        // receiver alonside us, the name will not include us. Actually, we can never
        // include as into a list.

        // So start off from collecting all emails from the chain (BTW, this can be cached,
        // I guess).

        auto participants = [this, &email, &references]() -> vector<types::EmailAddress> {
            vector<types::EmailAddress> result;

            // Note, it is not necessarry that we have all referenced emails in our internal
            // database. I suppose that when we gave been added into conversation later we may
            // still see all the references but don't have corresponding emails.
            // TODO: check it!
            for (auto& to : email.to) {
                result.emplace_back(to);
            }
            for (auto& from : email.from) {
                result.emplace_back(from);
            }

            for (auto& mid : references) {
                if (auto it = m_message_id_to_email_index.find(mid);
                    it != m_message_id_to_email_index.end()) {
                    result.emplace_back(it->second.from[0]);
                    for (auto& to : it->second.to) {
                        result.emplace_back(to);
                    }
                } else {
                    log_warning("referenced email with ID '{}' has not been found in the index",
                                mid);
                }
            }

            // Sort to make the list consistend for further comparison with existing nodes. It is
            // not only done as a prestep for erase algorithm.
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());

            // We leave self address only if there are not other people.
            if (result.size() > 1) {
                result.erase(std::remove(result.begin(), result.end(), m_own_address),
                             result.end());
            }

            return result;
        }();

        string group_folder_name = fmt::format("{}", fmt::join(participants, ", "));

        // We create a node for group folder with a semantics that it may exist if already
        // created.
        auto group_folder_node = create_path({group_folder_name});

        log_debug("created new folder with a name {}", group_folder_name);

        if (thread_id_opt.has_value()) {
            assert(thread_id_node);
            // update aggregated data
            // TODO: seems like we can have an index to exact node instead of having just a folder.x
            for (auto& c : thread_id_node->children) {
                if (c->ref.has_value() && c->ref.value().thread_id == thread_id_opt.value()) {
                    c->ref->emails_count += 1;
                    c->ref->attachments_count += email.attachments.size();
                    break;
                }
            }
            move_thread(thread_id_node, group_folder_node, thread_id_opt.value());

        } else {
            // this is new thread so we create it as a new thread in a new folder.
            auto new_thread_ref_node = create_thread_ref(
                group_folder_node, ThreadRef{.label = email.subject,
                                             .thread_id = email.message_id.value(),
                                             .emails_count = 1,
                                             .attachments_count = email.attachments.size()});
            m_message_to_tree_index[email.message_id.value()] = new_thread_ref_node;
            m_thread_id_to_tree_index[email.message_id.value()] = group_folder_node;
        }
    }

    // ThreadRef represents thread in a tree. From it we can render UI for thread. Consists of
    // subject and additional aggregated information.
    struct ThreadRef {
        string label;
        types::MessageID thread_id;
        size_t emails_count = 0;
        size_t attachments_count = 0;
    };

    void walk_tree_preoder(std::function<void(const string&)> enter_folder_cb,
                           std::function<void(const string&)> exit_folder_cb,
                           std::function<void(const ThreadRef&)> encounter_ref) const {
        walk_tree_preoder_it(&m_root, enter_folder_cb, exit_folder_cb, encounter_ref);
    }

   private:
    // Actually TreeNode is either Folder node (has label and children) or Leaf Node (has ref).
    struct TreeNode {
        // TODO: write destructor for a tree!
        string label;
        TreeNode* parent = nullptr;
        vector<TreeNode*> children;
        optional<ThreadRef> ref;

        void remove_child(TreeNode* child) {
            children.erase(std::remove(children.begin(), children.end(), child), children.end());
            delete child;
        }

        ~TreeNode() {
            for (auto c : children) {
                delete c;
            }
        }
    };

    void walk_tree_preoder_it(const TreeNode* node,
                              std::function<void(const string&)>& enter_folder_cb,
                              std::function<void(const string&)>& exit_folder_cb,
                              std::function<void(const ThreadRef&)>& encounter_ref) const {
        if (node->ref.has_value()) {
            encounter_ref(node->ref.value());
        } else {
            enter_folder_cb(node->label);
            for (auto* node : node->children) {
                walk_tree_preoder_it(node, enter_folder_cb, exit_folder_cb, encounter_ref);
            }
            exit_folder_cb(node->label);
        }
    }

    TreeNode* create_thread_ref(TreeNode* node, ThreadRef ref) {
        assert(node);
        node->children.emplace_back(new TreeNode{"", node, {}, std::move(ref)});
        return node->children.back();
    }

    // TODO: what is ThreadID, we need to find something for it. It can be a combination of initial
    // subject and maybe first message in it?
    void move_thread(TreeNode* from, TreeNode* to, types::MessageID thread_id) {
        assert(from);
        assert(to);

        log_debug("moving thread {} from node {} to node {}", thread_id, from->label, to->label);

        if (from == to) {
            log_debug("from == two case");
            return;
        }

        bool found = false;
        for (auto it = from->children.begin(); it != from->children.end();) {
            auto& c = *it;
            if (c->ref.has_value()) {
                // this is leaf children which holds a refernce to an email thread.
                if (c->ref.value().thread_id == thread_id) {
                    found = true;
                    log_debug("moving thread with ID {} to new destination", thread_id);
                    auto ref_id = create_thread_ref(to, std::move(c->ref.value()));
                    (void)(ref_id);
                    TreeNode* node = *it;
                    delete node;
                    it = from->children.erase(it);
                    log_debug("removing node  (children left: {})", from->children.size());

                    break;
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }

        if (!found) {
            log_warning(
                "thread with ID {} has not been found in source tree node and thus cannot be moved",
                thread_id);
        }

        if (from->children.empty()) {
            log_debug("removing folder {} as it is now empty", from->label);
            from->parent->remove_child(from);
        }
    }

    TreeNode* make_folders_path(vector<string> path) { return create_path_it(&m_root, path, 0); }

    TreeNode* create_path(const vector<string>& path) { return create_path_it(&m_root, path, 0); }

    TreeNode* create_path_it(TreeNode* node, const vector<string>& path, size_t component) {
        if (component == path.size()) {
            return node;
        }

        const auto& c = path[component];
        if (auto it = std::find_if(node->children.begin(), node->children.end(),
                                   [&](auto& n) { return n->label == c; });
            it == node->children.end()) {
            node->children.emplace_back(new TreeNode{c, node});
            return create_path_it(node->children.back(), path, component + 1);
        } else {
            return create_path_it(*it, path, component + 1);
        }
    }

    types::EmailAddress m_own_address;
    TreeNode m_root{"root"};
    map<MessageID, TreeNode*> m_message_to_tree_index;
    map<MessageID, types::MailboxEmail> m_message_id_to_email_index;
    map<MessageID, TreeNode*> m_thread_id_to_tree_index;
};

}  // namespace mailer
