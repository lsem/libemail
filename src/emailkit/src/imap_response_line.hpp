#pragma once

#include <emailkit/global.hpp>
#include <string>
#include <string_view>
#include "utils.hpp"

#include <iostream>

namespace emailkit {

struct imap_response_line_t {
    explicit imap_response_line_t(std::string line)
        : line(std::move(line)), tokens(utils::split_views(this->line, ' ')) {}

    imap_response_line_t() = default;

    const bool is_untagged_reply() const { return tokens.size() > 0 && tokens[0] == "*"; }

    const bool is_command_continiation_request() const {
        return tokens.size() > 0 && tokens[0] == "+";
    }

    const std::string line;
    const std::vector<std::string_view> tokens;
};

}  // namespace emailkit

DEFINE_FMT_FORMATTER(emailkit::imap_response_line_t, "{}", arg.line);
