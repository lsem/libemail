#pragma once

#include <asio/io_context.hpp>
#include <async_kit/async_callback.hpp>
#include <emailkit/log.hpp>
#include <memory>
#include <system_error>

using std::shared_ptr;

namespace asynckit = lsem::async_kit;

struct emailkit_log_fns {
    template <class... Args>
    static void print_error_line(std::string_view fmt_str, Args&&... args) {
        log_error("{}", fmt::vformat(fmt_str, fmt::make_format_args(args...)));
    }
};

template <class T>
using async_callback = lsem::async_kit::async_callback_impl_t<T, emailkit_log_fns>;

#include <fmt/core.h>
#include <fmt/ranges.h>

#define DEFINE_FMT_FORMATTER(Type, FmtString, ...)                                            \
    template <>                                                                               \
    struct fmt::formatter<Type> {                                                             \
        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {   \
            return ctx.end();                                                                 \
        }                                                                                     \
        auto format(const Type& arg, format_context& ctx) const -> format_context::iterator { \
            return fmt::format_to(ctx.out(), FmtString, __VA_ARGS__);                         \
        }                                                                                     \
    };

DEFINE_FMT_FORMATTER(std::error_code, "{}:{}", arg.category().name(), arg.message());