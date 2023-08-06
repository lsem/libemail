#pragma once
#include <string>
#include <vector>

namespace emailkit::utils {

inline std::string replace_control_chars(const std::string& s) {
    std::string r;
    for (auto& c : s) {
        if (c == '\r') {
            r += "\\r";
        } else if (c == '\n') {
            r += "\\n";
        } else {
            r += c;
        }
    }
    return r;
}

inline std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> r;
    bool in_word = false;
    size_t tok_start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delimiter) {
            if (in_word) {
                in_word = false;
                r.emplace_back(s, tok_start, i - tok_start);
            } else {
                // keep eating delimiters
            }
        } else {
            if (!in_word) {
                tok_start = i;
                in_word = true;
            }
        }
    }

    if (in_word) {
        r.emplace_back(s, tok_start);
    }

    return r;
}

}  // namespace emailkit::utils