#include "utils.hpp"

#include <b64/decode.h>
#include <b64/encode.h>

namespace emailkit::utils {

std::string replace_control_chars(const std::string& s) {
    std::string r;
    for (auto& c : s) {
        if (c == '\r') {
            r += "\\r";
        } else if (c == '\n') {
            r += "\\n";
        } else if (c == 0x01) {
            r += "^A";
        } else {
            r += c;
        }
    }
    return r;
}

std::string escape_ctrl(const std::string& s) {
    return replace_control_chars(s);
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> r;
    for (auto& tok : split_views(s, delimiter)) {
        r.emplace_back(std::string{tok});
    }
    return r;
}

std::vector<std::string_view> split_views(const std::string& s, char delimiter) {
    std::vector<std::string_view> r;
    bool in_word = false;
    size_t tok_start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delimiter) {
            if (in_word) {
                in_word = false;
                r.emplace_back(&s[tok_start], i - tok_start);
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
        r.emplace_back(&s[tok_start], s.length() - tok_start);
    }

    return r;
}

std::string base64_naive_decode(const std::string& s) {
    std::string res(base64::base64_decode_maxlength(s.size()), 0);
    base64::base64_decodestate state;
    base64::base64_init_decodestate(&state);
    const size_t output_size = base64::base64_decode_block(s.data(), s.size(), res.data(), &state);
    res.resize(output_size);
    return res;
}
std::string base64_naive_encode(const std::string& s) {
    std::string res(s.size() * 2, 0);
    base64::base64_encodestate state;
    base64::base64_init_encodestate(&state);
    size_t output_size = base64::base64_encode_block(s.data(), s.size(), res.data(), &state);
    output_size += base64::base64_encode_blockend(res.data() + output_size, &state);

    res.resize(output_size);

    return res;
}

}  // namespace emailkit::utils