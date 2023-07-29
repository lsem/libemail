
#pragma once
#include <string>

namespace emailkit::http_srv {
struct header {
    std::string name;
    std::string value;
};

}  // namespace emailkit::http_srv

#include <fmt/core.h>

template <>
struct fmt::formatter<emailkit::http_srv::header> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return ctx.end();
    }

    auto format(const emailkit::http_srv::header& r, format_context& ctx) const
        -> format_context::iterator {
        return fmt::format_to(ctx.out(), "header(name: {}, value: {})", r.name, r.value);
    }
};
