#include "utils.hpp"

#include "utf8_codec.hpp"

#include <b64/decode.h>
#include <b64/encode.h>

#include <utf7/utf7.h>

#include <optional>

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
        } else if (c == '\\') {
            r += "\\\\";
        } else if (c == '"') {
            r += "\\\"";
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

std::string strip(std::string s, char delimiter) {
    // lstrip
    ssize_t begin = 0, end = s.size();
    while (begin < s.size() && s[begin] == delimiter)
        begin++;

    while (end > 0 && s[end - 1] == delimiter)
        end--;

    return std::string(s, begin, end - begin);
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
            // return llvm::createStringError("incomplete input");
            log_error("incomplete input");
            return unexpected(make_error_code(std::errc::io_error));
        } else if (ret == UTF7_INVALID) {
            // return llvm::createStringError("invalid utf7 input");
            log_error("invalid utf7 input");
            return unexpected(make_error_code(std::errc::io_error));
        } else {
            utf8_codec::append_codepoint(res, ret);
        }
    }

    return res;
}

namespace {
std::optional<std::pair<std::string_view, std::string_view>> split2(std::string_view s,
                                                                    std::string_view tok) {
    using namespace std;
    // we need to find first occurence of tok in s.
    size_t tok_idx = s.find(tok);
    if (tok_idx != string_view::npos) {
        auto head = s;
        head.remove_suffix(s.size() - tok_idx);
        auto tail = s;
        tail.remove_prefix(tok_idx + tok.size());
        return pair{head, tail};
    } else {
        return std::nullopt;
    }
}
}  // namespace

bool can_be_mime_encoded_word(std::string_view s) {
    return s.size() > 4 && s[0] == '=' && s[1] == '?' && s[s.size() - 2] == '?' &&
           s[s.size() - 1] == '=';
}

expected<std::string> decode_mime_encoded_word(std::string s) {
    // Example: =?UTF-8?B?0J7QsdC70ZbQutC+0LLQuNC5INC30LDQv9C40YEg?=

    std::string result;

    std::string_view tail = s;

    while (!tail.empty()) {
        if (auto res = split2(tail, "=?")) {
            tail = std::get<1>(*res);
        } else {
            log_error("no starting =?");
            return unexpected(make_error_code(std::errc::io_error));
        }

        std::string charset, encoding, encoded_text;

        // Charset
        if (auto res = split2(tail, "?")) {
            tail = std::get<1>(*res);
            charset = std::get<0>(*res);
            std::transform(charset.begin(), charset.end(), charset.begin(), ::toupper);
        } else {
            log_error("no charset marker");
            return unexpected(make_error_code(std::errc::io_error));
        }

        // Encoding
        if (auto res = split2(tail, "?")) {
            tail = std::get<1>(*res);
            encoding = std::get<0>(*res);
            std::transform(encoding.begin(), encoding.end(), encoding.begin(), ::toupper);
        } else {
            log_error("no encoding marker");
            return unexpected(make_error_code(std::errc::io_error));
        }

        // Encoded text
        if (auto res = split2(tail, "?=")) {
            tail = std::get<1>(*res);
            encoded_text = std::get<0>(*res);
        } else {
            log_error("no encoding marker");
            return unexpected(make_error_code(std::errc::io_error));
        }

        log_debug("charset: '{}', encoding: '{}', text: '{}', tail: '{}'", charset, encoding,
                  encoded_text, tail);

        if (encoding == "B") {
            // BASE64
            if (charset == "UTF-8") {
                result += base64_naive_decode(std::string(encoded_text));
            } else {
                log_error("unsupported charset for mime encoded word: '{}'", charset);
                return unexpected(make_error_code(std::errc::io_error));
            }
        } else if (encoding == "Q") {
            log_error("Q encoding is not supported");
            return unexpected(make_error_code(std::errc::io_error));
        } else {
            log_error("unsupported encoding: '{}'", encoding);
            return unexpected(make_error_code(std::errc::io_error));
        }

        // In the beginning of the loop lets skip any whitespaces or CRLF. Normally, they should not
        // be there but if we hit limit of 75 characters servers usually break up and join. GMail
        // seems to be joining with SPACE only. But the spec says CRLF SPACE. Furthermore, on the
        // Internet there are examples where encoded words are concatenated without any delimiter at
        // all. Lets try to parse all these cases by having optional CRLF spaces in the beginning
        // for our grammar.
        // Section 2:
        //   An 'encoded-word' may not be more than 75 characters long, including
        //   'charset', 'encoding', 'encoded-text', and delimiters.  If it is
        //   desirable to encode more text than will fit in an 'encoded-word' of
        //   75 characters, multiple 'encoded-word's (separated by CRLF SPACE) may
        //   be used.

        if (auto res = split2(tail, "\r\n")) {
            tail = std::get<1>(*res);
        } else if (auto res = split2(tail, " ")) {
            tail = std::get<1>(*res);
        }
    }

    return result;
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
