#pragma once

#include <string>
#include <vector>

#include <emailkit/global.hpp>

namespace emailkit::imap_client::types {

struct list_response_entry_t {
    std::vector<std::string> inbox_path;
    std::vector<std::string> flags;
};

struct list_response_t {
    std::vector<list_response_entry_t> inbox_list;
};

}  // namespace emailkit::imap_client::types

DEFINE_FMT_FORMATTER(emailkit::imap_client::types::list_response_entry_t,
                     "{{inbox_path: {}, flags: {}}}",
                     arg.inbox_path,
                     arg.flags);

DEFINE_FMT_FORMATTER(emailkit::imap_client::types::list_response_t,
                     "list_response_t({})",
                     arg.inbox_list);
