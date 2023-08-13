#include "utf8_codec.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace emailkit::utf8_codec {

uint32_t select_bits(uint32_t number, unsigned from, unsigned to) {
    auto mask = static_cast<uint32_t>(0xFFFFFFFF) >> (31 - to);
    return (number & mask) >> from;
}

bool encode_point(uint32_t cp, char* buff, size_t buff_size, size_t& bytes_written) {
    assert(buff_size >= 4);

    if (cp <= 0x7F) {  // 7bit
        if (buff_size < 1) {
            return false;
        }
        buff[0] = static_cast<uint8_t>(cp);
        bytes_written = 1;
        return true;
    } else if (cp <= 0x7FF) {  // 11bit
        if (buff_size < 2) {
            return false;
        }
        buff[0] = 0xC0 | select_bits(cp, 6, 10);
        buff[1] = 0x80 | select_bits(cp, 0, 5);
        bytes_written = 2;
        return true;
    } else if (cp <= 0xFFFF) {  // 16bit
        if (buff_size < 3) {
            return false;
        }
        buff[0] = 0xE0 | select_bits(cp, 12, 15);
        buff[1] = 0x80 | select_bits(cp, 6, 11);
        buff[2] = 0x80 | select_bits(cp, 0, 5);
        bytes_written = 3;
        return true;
    } else if (cp <= 0x10FFFF) {  // 21 bit

        if (buff_size < 4) {
            return false;
        }
        buff[0] = 0xF0 | select_bits(cp, 18, 20);
        buff[1] = 0x80 | select_bits(cp, 12, 17);
        buff[2] = 0x80 | select_bits(cp, 6, 11);
        buff[3] = 0x80 | select_bits(cp, 0, 5);
        bytes_written = 4;
        return true;
    } else {
        bytes_written = 0;
        return false;
    }
}

bool append_codepoint(std::string& s, uint32_t codepoint) {
    std::array<char, 4> buff;
    size_t bytes_written;
    if (!encode_point(codepoint, buff.data(), buff.size(), bytes_written)) {
        return false;
    }
    s.append(buff.data(), bytes_written);
    return true;
}

}  // namespace emailkit::utf8_codec