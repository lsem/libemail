#pragma once
#include <emailkit/global.hpp>
#include <string>
#include <vector>

namespace emailkit::utils {

std::string replace_control_chars(const std::string& s);
std::string escape_ctrl(const std::string& s);
std::vector<std::string> split(const std::string& s, char delimiter);
std::vector<std::string_view> split_views(std::string_view s, char delimiter);
// std::vector<std::string_view> split_views(const std::string& s, char delimiter);

// TODO: accept string_view.
std::string base64_naive_decode(const std::string& s);
std::string base64_naive_encode(const std::string& s);

// Returns utf8.
expected<std::string> decode_imap_utf7(std::string s);
bool can_be_utf7_encoded_text(std::string_view s);

std::string_view strip_double_quotes(std::string_view s);

// sublime-text like match of one array against another
// (a, b, d) is subset match of (a, b, c, d)
template <class Collection1, class Collection2>
bool subset_match(const Collection1& c1, const Collection2& c2) {
    auto c2_curr = c2.begin();
    for (auto& e : c1) {
        c2_curr = std::find(c2_curr, c2.end(), e);
        if (c2_curr == c2.end()) {
            return false;
        }
    }
    return true;
}

template<class StringOrStringView>
bool starts_with(const StringOrStringView& s, std::string_view prefix) {
    return s.rfind(prefix) == 0;
}


}  // namespace emailkit::utils