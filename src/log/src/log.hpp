#pragma once

#include <experimental/source_location>
#include <fmt/color.h>
#include <fmt/format.h>

struct fmt_and_location {
  std::string_view fmt;
  std::experimental::source_location location;

  template <typename S>
  fmt_and_location(const S &fmt,
                   const std::experimental::source_location &location =
                       std::experimental::source_location::current())
      : fmt(fmt), location(location) {}
};

enum class log_level_t {
  info,
  debug,
  warning,
  error,
};

template <class... Args>
void log_impl(log_level_t level, fmt_and_location fmt, fmt::format_args args) {
  const auto style = ([level] {
    switch (level) {
    case log_level_t::debug:
      return fmt::fg(fmt::color::gray);
    case log_level_t::info:
      return fmt::fg(fmt::color::light_gray);
    case log_level_t::warning:
      return fmt::bg(fmt::color::yellow);
    case log_level_t::error:
      return fmt::bg(fmt::color::indian_red) | fmt::fg(fmt::color::white);
    }
    return fmt::text_style{};
  })();
  fmt::print(stdout, style, "{}: {}: ", fmt.location.file_name(),
             fmt.location.line());
  fmt::vprint(stdout, style, fmt.fmt, args);
  fmt::print(stdout, "\n");
}

template <class... Args> void log_debug(fmt_and_location fmt, Args &&... args) {
  log_impl(log_level_t::debug, std::move(fmt), fmt::make_format_args(args...));
}
template <class... Args> void log_info(fmt_and_location fmt, Args &&... args) {
  log_impl(log_level_t::info, std::move(fmt), fmt::make_format_args(args...));
}
template <class... Args>
void log_warning(fmt_and_location fmt, Args &&... args) {
  log_impl(log_level_t::warning, std::move(fmt),
           fmt::make_format_args(args...));
}
template <class... Args> void log_error(fmt_and_location fmt, Args &&... args) {
  log_impl(log_level_t::error, std::move(fmt), fmt::make_format_args(args...));
}
