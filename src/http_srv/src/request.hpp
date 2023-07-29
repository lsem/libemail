#pragma once
#include <string>
#include <vector>
#include "header.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>

namespace emailkit::http_srv {

/// A request received from a client.
struct request {
    std::string method;
    std::string uri;
    int http_version_major;
    int http_version_minor;
    std::vector<header> headers;
};
}  // namespace emailkit::http_srv

// TODO: employ approach where we have printers in one place.
template <>
struct fmt::formatter<emailkit::http_srv::request> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return ctx.end();
    }

    auto format(const emailkit::http_srv::request& r, format_context& ctx) const -> format_context::iterator {
        return fmt::format_to(
            ctx.out(), "request(method: {}, uri: {}, major: {}, minor: {}, headers: {})", r.method,
            r.uri, r.http_version_major, r.http_version_minor, r.headers);
    }
};
