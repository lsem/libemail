#pragma once
#include <string>
#include <vector>

namespace emailkit::utils {

std::string replace_control_chars(const std::string& s);
std::string escape_ctrl(const std::string& s);
std::vector<std::string> split(const std::string& s, char delimiter);
std::vector<std::string_view> split_views(std::string_view s, char delimiter);
//std::vector<std::string_view> split_views(const std::string& s, char delimiter);
std::string base64_naive_decode(const std::string& s);
std::string base64_naive_encode(const std::string& s);

}  // namespace emailkit::utils