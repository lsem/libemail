#include <emailkit/utils.hpp>

#include <gtest/gtest.h>

#include <fmt/format.h>

namespace {
unsigned char rnd_100_bytes[] = {
    0x31, 0x92, 0x4a, 0xe4, 0x7f, 0xe0, 0xe8, 0xca, 0xe6, 0x64, 0x1c, 0x21, 0x9c, 0xfc, 0xf1,
    0xd3, 0xad, 0xf8, 0xa0, 0x85, 0xae, 0x3c, 0x72, 0xdb, 0xf5, 0x51, 0x78, 0x46, 0xff, 0xb9,
    0xf6, 0xbc, 0x56, 0x58, 0x0c, 0x26, 0x70, 0x83, 0xba, 0x68, 0x58, 0x8e, 0x44, 0x96, 0xf6,
    0x28, 0xf6, 0x23, 0x76, 0x08, 0xa6, 0x05, 0x9d, 0x8a, 0x4c, 0x35, 0x3c, 0xf9, 0xfc, 0xf2,
    0x80, 0x50, 0xdf, 0xe8, 0x9a, 0x1e, 0x8c, 0x44, 0x68, 0x83, 0xa3, 0xac, 0x52, 0xfe, 0x82,
    0xcd, 0xba, 0xc4, 0x7d, 0xc1, 0x5e, 0x92, 0x9f, 0xf2, 0x80, 0xbd, 0xa8, 0x83, 0x9c, 0x54,
    0xe6, 0x9c, 0x00, 0xdc, 0xb8, 0xe2, 0x09, 0x8d, 0x43, 0xf7};
unsigned int rnd_100_bytes_len = 100;
const std::string random_bytes{rnd_100_bytes, rnd_100_bytes + rnd_100_bytes_len};

TEST(utils_test, base64_encode_basic_test) {
    EXPECT_EQ(emailkit::utils::base64_naive_encode("test"), "dGVzdA==");

    EXPECT_EQ(emailkit::utils::base64_naive_encode(random_bytes),
              "MZJK5H/"
              "g6MrmZBwhnPzx0634oIWuPHLb9VF4Rv+59rxWWAwmcIO6aFiORJb2KPYjdgimBZ2KTDU8+"
              "fzygFDf6JoejERog6OsUv6CzbrEfcFekp/ygL2og5xU5pwA3LjiCY1D9w==");
}

TEST(utils_test, base64_encode_decode_roundtrip_test) {
    std::vector<std::string> samples;
    samples.emplace_back("example test with control chars just in case \001");
    samples.emplace_back("");
    samples.emplace_back("\x1");
    samples.emplace_back("\x1\x1");
    samples.emplace_back(random_bytes);

    for (auto& s : samples) {
        auto encoded = emailkit::utils::base64_naive_encode(s);
        auto decoded = emailkit::utils::base64_naive_decode(encoded);
        EXPECT_EQ(s, decoded) << fmt::format("encoded: '{}', decoded: '{}', sample: '{}'", encoded,
                                             emailkit::utils::escape_ctrl(decoded),
                                             emailkit::utils::escape_ctrl(s));
    }
}

}  // namespace
