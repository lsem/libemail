#pragma once

#include <asio/io_context.hpp>
#include <async_kit/async_callback.hpp>
#include <memory>
#include <system_error>

using std::shared_ptr;

namespace asynckit = lsem::async_kit;
using asynckit::async_callback;

#include <fmt/core.h>

template <>
struct fmt::formatter<std::error_code> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return ctx.end();
    };

    auto format(const std::error_code& ec, format_context& ctx) const -> format_context::iterator {
        return fmt::format_to(ctx.out(), "{}:{}", ec.category().name(), ec.message());
    }
};