#pragma once

#include <fmt/color.h>
#include <fmt/format.h>
#include <chrono>

#include <string_view>

#include <unistd.h>

enum class log_level_t {
    error,
    warning,
    info,
    debug,
};

extern log_level_t g_current_level;

constexpr std::string_view strip_fpath(std::string_view fpath) {
    size_t last_slash_pos = 0;
    for (size_t i = 0; i < fpath.size(); ++i) {
        if (fpath[i] == '/') {
            last_slash_pos = i;
        }
    }
    fpath.remove_prefix(last_slash_pos + 1);
    return fpath;
}

static_assert(strip_fpath("a/b/c") == "c");

template <class... Args>
void log_impl(log_level_t level,
              int line,
              const char* file_name,
              std::string_view fmt,
              Args... args) {
    static bool log_level_read = false;
    if (!log_level_read) {
        std::string level_val;
        if (std::getenv("LOG")) {
            level_val = std::getenv("LOG");
            if (level_val == "debug") {
                g_current_level = log_level_t::debug;
            } else if (level_val == "info") {
                g_current_level = log_level_t::info;
            } else if (level_val == "warning") {
                g_current_level = log_level_t::warning;
            } else if (level_val == "error") {
                g_current_level = log_level_t::error;
            }
        } else if (std::getenv("DEBUG")) {
            g_current_level = log_level_t::debug;
        }
        log_level_read = true;
    }

    if (static_cast<int>(level) > static_cast<int>(g_current_level)) {
        return;
    }

    const auto at_tty = isatty(STDOUT_FILENO);
    const auto style = ([level, at_tty] {
        if (!at_tty) {
            return fmt::text_style{};
        }
        switch (level) {
            case log_level_t::debug:
                return fmt::fg(fmt::color::light_gray);
            case log_level_t::info:
                return fmt::fg(fmt::color::gray);
            case log_level_t::warning:
                return fmt::bg(fmt::color::yellow) | fmt::fg(fmt::color::black);
            case log_level_t::error:
                return fmt::bg(fmt::color::indian_red) | fmt::fg(fmt::color::white);
        }
        return fmt::text_style{};
    })();

    const auto lvl_s = [level]() -> std::string_view {
        switch (level) {
            case log_level_t::debug:
                return "DBG";
            case log_level_t::info:
                return "INF";
            case log_level_t::warning:
                return "WRN";
            case log_level_t::error:
                return "ERR";
            default:
                return "UNK";
        }
    }();

    static const auto local_epooch = std::chrono::steady_clock::now();
    auto curr_ms = (std::chrono::steady_clock::now() - local_epooch) / std::chrono::milliseconds(1);

    fmt::print(stdout, style, "{:<4}:  {}  ", curr_ms, lvl_s);
    fmt::vprint(stdout, style, fmt, fmt::make_format_args(args...));
    fmt::print(stdout, style, " ({}:{}) ", file_name, /*strip_fpath(file_name),*/ line);
    fmt::print(stdout, "\n");
}

#define log_error(Fmt, ...) \
    log_impl(log_level_t::error, __LINE__, __FILE__, Fmt __VA_OPT__(, ) __VA_ARGS__)
#define log_warning(Fmt, ...) \
    log_impl(log_level_t::warning, __LINE__, __FILE__, Fmt __VA_OPT__(, ) __VA_ARGS__)
#define log_info(Fmt, ...) \
    log_impl(log_level_t::info, __LINE__, __FILE__, Fmt __VA_OPT__(, ) __VA_ARGS__)
#define log_debug(Fmt, ...) //\
    //log_impl(log_level_t::debug, __LINE__, __FILE__, Fmt __VA_OPT__(, ) __VA_ARGS__)
