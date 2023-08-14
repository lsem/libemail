#include "utils.hpp"

#include "utf8_codec.hpp"

#include <b64/decode.h>
#include <b64/encode.h>

#include <utf7/utf7.h>

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

std::vector<std::string_view> split_views(std::string_view s, char delimiter) {
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

expected<std::string> decode_imap_utf7(std::string s) {
    // https://datatracker.ietf.org/doc/html/rfc3501#section-5.1.3
    // https://datatracker.ietf.org/doc/html/rfc2152
    // https://crawshaw.io/blog/utf7

    if (s.empty() || s[0] != '&') {
        // return llvm::createStringError("not valid imap-utf7 encoded input");
        log_error("not valid imap-utf7 encoded input: '{}'", s);
        return unexpected(make_error_code(std::errc::io_error));
    }

    for (auto& c : s) {
        if (c == '/') {
            c = ',';
        }
        // TODO: check this
        // It it possible that correct way to do this is to replace only first character '&-'>'+'
        // but for the sake of simplicity I do it like this for now. For gmail it seems to be
        // working.
        if (c == '&') {
            c = '+';
        }
    }

    struct utf7 ctx;

    utf7_init(&ctx, nullptr);
    ctx.buf = s.data();
    ctx.len = s.size();

    std::string res;

    while (true) {
        long ret = utf7_decode(&ctx);

        if (ret == UTF7_OK) {
            break;
        } else if (ret == UTF7_INCOMPLETE) {
            //return llvm::createStringError("incomplete input");
            log_error("incomplete input");
            return unexpected(make_error_code(std::errc::io_error));
        } else if (ret == UTF7_INVALID) {
            //return llvm::createStringError("invalid utf7 input");
            log_error("invalid utf7 input");
            return unexpected(make_error_code(std::errc::io_error));
        } else {
            utf8_codec::append_codepoint(res, ret);
        }
    }

    return res;
}

bool can_be_utf7_encoded_text(std::string_view s) {
    return s.size() > 2 && s[0] == '&' && s[s.size() - 1] == '-';
}

std::string_view strip_double_quotes(std::string_view s) {
    if (s.size() >= 2 && s[0] == '"' && s[s.size() - 1] == '"') {
        s.remove_suffix(1);
        s.remove_prefix(1);
        return s;
    } else {
        return s;
    }
}

}  // namespace emailkit::utils
