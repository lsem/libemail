#pragma once

#include <asio/io_context.hpp>
#include <async_kit/async_callback.hpp>
#include <chrono>
#include <emailkit/log.hpp>
#include <memory>
#include <system_error>
#include <tl/expected.hpp>
#include <type_traits>

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <optional>
#include <variant>

// Lets pretend we have "normal" language.
using std::list;
using std::map;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;
using std::optional;
using std::variant;

/////////////////////////////////////////////////////////////////////

template <class T>
using expected = tl::expected<T, std::error_code>;
using unexpected = tl::unexpected<std::error_code>;

using namespace std::literals;
using namespace std::literals::chrono_literals;

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

/////////////////////////////////////////////////////////////////////
// EnableUseThis
template <class T>
class EnableUseThis : public std::enable_shared_from_this<T> {
   public:
    template <class Cb, class Fn>
    auto use_this(Cb cb, Fn fn) {
        auto this_weak = EnableUseThis<T>::weak_from_this();
        if constexpr (std::is_invocable_v<Fn, T&, std::error_code, Cb>) {
            return [cb = std::move(cb), this_weak = std::move(this_weak),
                    fn = std::move(fn)](std::error_code ec) mutable {
                auto this_shared = this_weak.lock();
                if (!this_shared) {
                    if constexpr (std::is_invocable_v<decltype(cb), std::error_code>) {
                        cb(make_error_code(std::errc::owner_dead));
                    } else {
                        cb(make_error_code(std::errc::owner_dead), {});
                    }
                } else {
                    fn(*this_shared, ec, std::move(cb));
                }
            };
        } else {
            return [cb = std::move(cb), this_weak = std::move(this_weak), fn = std::move(fn)](
                       std::error_code ec, auto r) mutable {
                auto this_shared = this_weak.lock();
                if (!this_shared) {
                    if constexpr (std::is_invocable_v<decltype(cb), std::error_code>) {
                        cb(make_error_code(std::errc::owner_dead));
                    } else {
                        cb(make_error_code(std::errc::owner_dead), {});
                    }
                } else {
                    fn(*this_shared, ec, std::move(r), std::move(cb));
                }
            };
        }
    }
};

#define ASYNC_RETURN_ON_ERROR(Ec, Cb, Msg)                                    \
    do {                                                                      \
        if (ec) {                                                             \
            log_error("{}: {}", Msg, ec);                                     \
            if constexpr (std::is_invocable_v<decltype(Cb), std::error_code>) \
                cb(ec);                                                       \
            else                                                              \
                cb(ec, {});                                                   \
            return;                                                           \
        }                                                                     \
    } while (false)

template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overload(Ts...) -> overload<Ts...>;

#define PROPAGATE_ERROR_VIA_CB(ec, Message, Cb) \
    do {                                        \
        if (ec) {                               \
            log_error("{}: {}", Message, ec);   \
            details::call_cb(Cb, ec);           \
            return;                             \
        }                                       \
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
