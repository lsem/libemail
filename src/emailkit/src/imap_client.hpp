#pragma once
#include <emailkit/global.hpp>
#include <asio/io_context.hpp>

#include <functional>
#include <memory>
#include <vector>

#include "imap_client_types.hpp"
#include "types.hpp"

namespace emailkit::imap_client {

enum class imap_client_state {
    not_connected,
    connected,
    authorized,
};

struct xoauth2_creds_t {
    std::string user_email;
    std::string oauth_token;
};

struct oauthbearer_creds_t {
    std::string user_email;
    std::string host;
    std::string port;
    std::string oauth_token;
};

struct auth_error_details_t {
    std::string summary;
};

namespace imap_commands {
struct namespace_t {};

struct list_t {
    std::string reference_name;
    std::string mailbox_name;  // with possible wildcards.
};

struct select_t {
    std::string mailbox_name;
};

//
// Defines of one arguments that can be passed to FETCH command.
// See FETCH Command in RFC for details:
// (https://datatracker.ietf.org/doc/html/rfc3501#section-6.4.5
// Note, not for all command there is parser.
namespace fetch_items {
struct body_t {};

// TODO: not supported yet, encoding is complicated here.
// struct body_part_t {
//     std::string section_spec;
//     std::string partial;
// };
// struct body_peek_t {
//     std::string section_spec;
//     std::string partial;
// };

struct body_structure_t {};
struct envelope_t {};
struct flags_t {};
struct internal_date_t {};
struct rfc822_t {};
struct rfc822_header_t {};
struct rfc822_size_t {};
struct rfc822_text_t {};
struct uid_t {};

struct raw {
    std::string idname;
};

}  // namespace fetch_items

// Can be used if none of the above suits or implemented incorrectly.
struct all_t {};
struct fast_t {};
struct full_t {};

using fetch_items_raw_string_t = std::string;
using fetch_item_t = std::variant<fetch_items::body_t,
                                  //   fetch_items::body_part_t,
                                  //   fetch_items::body_peek_t,
                                  fetch_items::body_structure_t,
                                  fetch_items::envelope_t,
                                  fetch_items::flags_t,
                                  fetch_items::internal_date_t,
                                  fetch_items::rfc822_t,
                                  fetch_items::rfc822_header_t,
                                  fetch_items::rfc822_size_t,
                                  fetch_items::rfc822_text_t,
                                  fetch_items::uid_t,
                                  fetch_items_raw_string_t>;
using fetch_items_vec_t = std::vector<fetch_item_t>;

struct fetch_sequence_spec {
    int from{};
    int to{};
};
using raw_fetch_sequence_spec = std::string;

struct fetch_t {
    // 2,4:7,9,12:* -> 2,4,5,6,7,9,12,13,14,15 -- for mailbox of size 15.
    std::variant<fetch_sequence_spec, raw_fetch_sequence_spec> sequence_set;
    std::variant<all_t, fast_t, full_t, fetch_items_vec_t> items;
};

expected<std::string> encode_cmd(const fetch_t& cmd);

}  // namespace imap_commands

// NOTE: this is not just imap_client in a way that it is imap protocol client. But it is IMAP Email
// client. It provides both low level IMAP commands that directly represents IMAP RFC and can be
// used for trying out some things on the server . And, a bit more capable commands that execute
// IMAP commands underneath and make additional parsing like RFC822 so that we can build high-level
// commands on top of that almost without additonal work except maybe some remapping. Note, one
// should sill understand how IMAP works, its stateful nature and all pecularities of it. The class
// is not smart in any way.
class imap_client_t {
   public:
    virtual ~imap_client_t() = default;
    virtual void start() = 0;
    virtual void on_state_change(std::function<void(imap_client_state)>) = 0;

   public:  // IMAP protocol commands
    virtual void async_connect(std::string host, std::string port, async_callback<void> cb) = 0;
    virtual void async_obtain_capabilities(async_callback<std::vector<std::string>> cb) = 0;
    virtual void async_authenticate(xoauth2_creds_t creds,
                                    async_callback<auth_error_details_t> cb) = 0;
    virtual void async_execute_command(imap_commands::namespace_t, async_callback<void> cb) = 0;
    virtual void async_execute_command(imap_commands::list_t,
                                       async_callback<types::list_response_t> cb) = 0;
    virtual void async_execute_command(imap_commands::select_t,
                                       async_callback<types::select_response_t> cb) = 0;
    virtual void async_execute_command(imap_commands::fetch_t,
                                       async_callback<types::fetch_response_t> cb) = 0;
    // TODO: https://www.rfc-editor.org/rfc/rfc7628.html
    // virtual void async_authenticate(oauthbearer_creds_t creds,
    //                                 async_callback<auth_error_details_t> cb) {}

   public:  // Higher level functions that execute standard IMAP commands and do additional parsing
            // where needed.
    struct ListMailboxesResult {
        types::list_response_t raw_response;  // unprocessed response
    };
    virtual void async_list_mailboxes(async_callback<ListMailboxesResult> cb) = 0;

    struct SelectMailboxResult {
        // Parsed server response without any interpretation
        types::select_response_t raw_response;
        // unsigned int exists{};
        // unsigned int recents{};
        // unsigned uid_validity{};
        // unsigned unseen{};
        // unsigned uidnext{};
        // unsigned highestmodseq{};
    };
    virtual void async_select_mailbox(std::string inbox_name,
                                      async_callback<SelectMailboxResult> cb) = 0;

    virtual void async_list_items(
        int from,
        std::optional<int> to,
        async_callback<std::variant<string, std::vector<emailkit::types::MailboxEmail>>> cb) = 0;

   public:
    // TODO: state change API (logical states + disconnected/failed)
};

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx);

// test client has predictive tags: a1, a2, a_n..
std::shared_ptr<imap_client_t> make_test_imap_client(asio::io_context& ctx);

}  // namespace emailkit::imap_client
