#include <emailkit/global.hpp>
#include <emailkit/log.hpp>

#include "imap_parser_utils.hpp"
#include "utils.hpp"

namespace emailkit::imap_parser::utils {

std::vector<std::string> decode_mailbox_path_from_list_response(const list_response_t& r) {
    std::vector<std::string> decoded_path_tokens;

    if (!r.hierarchy_delimiter.empty()) {
        if (r.hierarchy_delimiter.size() != 1) {
            log_warning("we can't split multichar hierarchy delimiters, skipping");
            return {};
        }

        log_debug("r.hierarchy_delimiter[0]: '{}'", r.hierarchy_delimiter[0]);
        auto tokens = emailkit::utils::split_views(r.mailbox, r.hierarchy_delimiter[0]);

        for (auto& tok : tokens) {
            if (emailkit::utils::can_be_utf7_encoded_text(tok)) {
                auto utf8_or_err = emailkit::utils::decode_imap_utf7(std::string(tok));
                if (!utf8_or_err) {
                    log_error("failed parsing token: '{}': {}", tok, utf8_or_err.error());
                    continue;
                }
                decoded_path_tokens.emplace_back(std::move(*utf8_or_err));
            } else {
                decoded_path_tokens.emplace_back(tok);
            }
        }

        log_info("decoded path: {}", decoded_path_tokens);
    }

    return decoded_path_tokens;
}

}  // namespace emailkit::imap_parser::utils