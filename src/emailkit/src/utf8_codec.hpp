#pragma once

#include <cstdint>
#include <string>

namespace emailkit::utf8_codec {

bool encode_point(uint32_t cp, char* buff, size_t buff_size, size_t& bytes_written);
bool append_codepoint(std::string& s, uint32_t codepoint);

}  // namespace emailkit::utf8_codec