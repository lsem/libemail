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
        if (!email.is_valid) {
            auto group_folder_node = create_path({"INTERNAL", "Invalid Messages"});
            create_thread_ref(
                group_folder_node,
                ThreadRef{
                    .label = email.subject,
                    // Use message ID of the the first message we got for this thread as ThreadID.
                    .thread_id = "",
                    .emails_count = 0,
                    .attachments_count = 0});

            return;
        }

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

        // Index

        if (m_message_id_to_email_index.find(email.message_id.value()) !=
            m_message_id_to_email_index.end()) {
            log_warning("message with ID {} already exists in the index", email.message_id.value());
            return;
        }
        m_message_id_to_email_index[email.message_id.value()] = email;
        log_debug("email with ID '{}' added to the index", email.message_id.value());

        log_debug("processing email:\n{}", types::to_json(email));

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
            log_debug(
                "message with ID {} is considered to be part of the thread with ID {}, node: {}",
                *thread_id_opt, email.message_id.value(), static_cast<void*>(thread_id_node));
        }

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

            log_debug("removing own address '{}' from the list {}", m_own_address, result);

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

        log_debug("created (or alreayd have) a folder with a name {}", group_folder_name);

        if (thread_id_opt) {
            assert(thread_id_node);
            auto& thread_id = *thread_id_opt;

            // update aggregate data

            if (auto it = m_thread_id_to_tree_index.find(thread_id);
                it != m_thread_id_to_tree_index.end()) {
                TreeNode* node = it->second;
                assert(node);
                if (auto t_it = node->find_thread_by_id(thread_id);
                    t_it != node->thread_refs_end()) {
                    t_it->emails_count += 1;
                    t_it->attachments_count += email.attachments.size();
                } else {
                    log_error("could not find thread {} to update aggregate data", thread_id);
                }
            }

            auto new_location = move_thread(thread_id_node, group_folder_node, *thread_id_opt);

            m_thread_id_to_tree_index.erase(*thread_id_opt);
            m_thread_id_to_tree_index.emplace(*thread_id_opt, group_folder_node);

            if (new_location) {
                // m_message_to_tree_index.erase(*thread_id_opt);
                // m_message_to_tree_index.emplace(*thread_id_opt, group_folder_node);
            }
        } else {
            // this is new thread so we create it as a new thread in a new folder.
            create_thread_ref(
                group_folder_node,
                ThreadRef{
                    .label = email.subject,
                    // Use message ID of the the first message we got for this thread as ThreadID.
                    .thread_id = email.message_id.value(),
                    .emails_count = 1,
                    .attachments_count = email.attachments.size()});
            m_thread_id_to_tree_index[email.message_id.value()] = group_folder_node;
            //            m_message_to_tree_index[email.message_id.value()] = group_folder_node;
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

   public:
    // TreeNode is either Folder node (has label and children) or Leaf Node (has ref).
    struct TreeNode {
        string label;
        TreeNode* parent = nullptr;
        vector<TreeNode*> children;
        vector<ThreadRef> threads_refs;

        TreeNode() = delete;

        explicit TreeNode(string label) : label(std::move(label)) {
            log_info("created node {}", (void*)this);
        }

        explicit TreeNode(string label, TreeNode* parent, vector<TreeNode*> children)
            : label(std::move(label)), parent(parent), children(std::move(children)) {
            log_info("created node {}", (void*)this);
        }

        ~TreeNode() {
            log_info("deleting node {}", (void*)this);
            for (auto c : children) {
                delete c;
            }
        }

        TreeNode(const TreeNode&) = delete;
        TreeNode& operator=(const TreeNode&) = delete;

        void remove_child(TreeNode* child) {
            children.erase(std::remove(children.begin(), children.end(), child), children.end());
            log_debug("deleted node {}", (void*)child);
            delete child;
        }

        using ThreadsIterator = vector<ThreadRef>::iterator;

        ThreadsIterator find_thread_by_id(const MessageID& id) {
            return std::find_if(threads_refs.begin(), threads_refs.end(),
                                [&id](auto& x) { return x.thread_id == id; });
        }
        ThreadsIterator thread_refs_end() { return threads_refs.end(); }
    };

    void walk_tree_preoder_it(const TreeNode* node,
                              std::function<void(const string&)>& enter_folder_cb,
                              std::function<void(const string&)>& exit_folder_cb,
                              std::function<void(const ThreadRef&)>& encounter_ref) const {
        enter_folder_cb(node->label);
        for (auto* node : node->children) {
            walk_tree_preoder_it(node, enter_folder_cb, exit_folder_cb, encounter_ref);
        }
        for (auto& thread_ref : node->threads_refs) {
            encounter_ref(thread_ref);
        }
        exit_folder_cb(node->label);
    }

    void create_thread_ref(TreeNode* node, ThreadRef ref) {
        assert(node);
        node->threads_refs.emplace_back(std::move(ref));
    }

    TreeNode* move_thread(TreeNode* from, TreeNode* to, types::MessageID thread_id) {
        assert(from);
        assert(to);

        TreeNode* result = nullptr;

        log_debug("moving thread {} from node {} to node {}", thread_id, from->label, to->label);

        if (from == to) {
            log_debug("from == two case");
            return nullptr;
        }

        // The task is to find a thread in FROM TreeNode and remove it from there.

        // TODO: consider having some kind of index here. Some folders may be (1k-1k children)
        // we have this loop because we don't know the position in our children array. Effective
        // index may be not possible or will be non-trivial because in naive index after removing
        // element entire index will be invalidated.
        bool found = false;
        for (auto it = from->threads_refs.begin(); it != from->threads_refs.end(); ++it) {
            auto& c = *it;
            if (c.thread_id == thread_id) {
                found = true;
                log_debug("moving thread with ID {} to new destination", thread_id);
                create_thread_ref(to, std::move(c));
                it = from->threads_refs.erase(it);
                log_debug("removing node  (children left: {})", from->threads_refs.size());
                break;
            }
        }

        if (!found) {
            log_warning(
                "thread with ID {} has not been found in source tree node and thus cannot be moved",
                thread_id);
        }

        if (from->children.empty() && from->threads_refs.empty()) {
            log_debug("removing folder {} as it is now empty", from->label);
            from->parent->remove_child(from);
        }

        return result;
    }

    TreeNode* create_path(const vector<string>& path) { return create_path_it(&m_root, path, 0); }

    TreeNode* create_path_it(TreeNode* node, const vector<string>& path, size_t component) {
        if (component == path.size()) {
            return node;
        }

        const auto& c = path[component];
        if (auto it = std::find_if(node->children.begin(), node->children.end(),
                                   [&](auto& n) { return n->label == c; });
            it == node->children.end()) {
            node->children.emplace_back(new TreeNode{c, node, {}});
            return create_path_it(node->children.back(), path, component + 1);
        } else {
            return create_path_it(*it, path, component + 1);
        }
    }

   public:
    types::EmailAddress m_own_address;
    TreeNode m_root{"root"};
    map<MessageID, types::MailboxEmail> m_message_id_to_email_index;
    //    map<MessageID, TreeNode*> m_message_to_tree_index;
    map<MessageID, TreeNode*> m_thread_id_to_tree_index;
};

}  // namespace mailer
