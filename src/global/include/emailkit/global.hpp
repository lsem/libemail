#pragma once

#include <asio/io_context.hpp>
#include <async_kit/async_callback.hpp>
#include <emailkit/log.hpp>
#include <memory>
#include <system_error>
#include <type_traits>

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

namespace details {
template <class T>
void call_cb(async_callback<T>& cb, std::error_code ec) {
    if constexpr (std::is_void_v<T>) {
        cb(ec);
    } else {
        cb(ec, T{});
    }
}
}  // namespace details

#define PROPAGATE_ERROR_VIA_CB(ec, Message, Cb)      \
    do {                                      \
        if (ec) {                             \
            log_error("{}: {}", Message, ec); \
            details::call_cb(Cb, ec);         \
            return;                           \
        }                                     \
    } while (false)

//////////////////////////////////////////////////////////////////////////////////////////

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