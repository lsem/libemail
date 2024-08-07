#pragma once

#include <emailkit/global.hpp>
#include <string>
#include <string_view>
#include "utils.hpp"

#include <iostream>

namespace emailkit {

namespace details {

inline std::string_view remove_crln(std::string_view s) {
    if (s.size() >= 2 && s[s.size() - 2] == '\r' && s[s.size() - 1] == '\n') {
        s.remove_suffix(2);
        return s;
    } else {
        return s;
    }
}
}  // namespace details

struct imap_response_line_t {
    explicit imap_response_line_t(std::string line)
        : line(std::move(line)),
          tokens(utils::split_views(details::remove_crln(this->line), ' ')) {}

    imap_response_line_t() = default;

    const bool is_untagged_reply() const {
        return this->line.size() > 1 && this->line[0] == '*' && this->line[1] == ' ';
    }

    // removed * from reply and strips down \r\n, result can be given to parser.
    // TODO: unit test for this.
    std::string_view unwrap_untagged_reply() {
        if (!is_untagged_reply()) {
            return {};
        }

        log_debug("is_untagged reply, unwrapping");

        // TODO: ensure there is CRLF in the end.
        std::string_view result = this->line;
        result.remove_prefix(2);

        if (result.size() >= 2 && result[result.size() - 2] == '\r' &&
            result[result.size() - 1] == '\n') {
            log_debug("removing suffux");
            result.remove_suffix(2);
        }

        log_debug("result is: '{}'", result);

        return result;
    }

    const bool is_command_continiation_request() const {
        return tokens.size() > 0 && tokens[0] == "+";
    }

    bool first_token_is(std::string_view s) const { return tokens.size() > 0 && tokens[0] == s; }

    bool is_ok_response() const { return tokens.size() >= 2 && tokens[1] == "OK"; }
    bool is_no_response() const { return tokens.size() >= 2 && tokens[1] == "NO"; }
    bool is_bad_response() const { return tokens.size() >= 2 && tokens[1] == "BAD"; }

    bool maybe_tagged_reply() const {
        return !is_command_continiation_request() && !is_untagged_reply() && tokens.size() > 1;
    }

    const std::string line;
    const std::vector<std::string_view> tokens;
};

}  // namespace emailkit

DEFINE_FMT_FORMATTER(emailkit::imap_response_line_t,
                     "'{}'",
                     emailkit::utils::escape_ctrl(arg.line));
