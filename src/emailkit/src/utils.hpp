#pragma once
#include <llvm_expected.hpp>
#include <string>
#include <vector>

namespace emailkit::utils {

std::string replace_control_chars(const std::string& s);
std::string escape_ctrl(const std::string& s);
std::vector<std::string> split(const std::string& s, char delimiter);
std::vector<std::string_view> split_views(std::string_view s, char delimiter);
//std::vector<std::string_view> split_views(const std::string& s, char delimiter);

// TODO: accept string_view.
std::string base64_naive_decode(const std::string& s);
std::string base64_naive_encode(const std::string& s);

// Returns utf8.
llvm::Expected<std::string> decode_imap_utf7(std::string s);
bool can_be_utf7_encoded_text(std::string_view s);

std::string_view strip_double_quotes(std::string_view s);


}  // namespace emailkit::utils