#include "imap_parser_types.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace emailkit::imap_parser {

std::string to_json(const list_response_t& x) {
    return fmt::format(R"json(
        {{
            "mailbox": "{}",
            "mailbox_list_flags": {},
            "hierarchy_delimiter": "{}"
        }}
    )json",
                       x.mailbox, x.mailbox_list_flags, x.hierarchy_delimiter);
}

namespace {
class parser_err_category_t : public std::error_category {
   public:
    const char* name() const noexcept override { return "imap_parser_err"; }
    std::string message(int ev) const override {
        switch (static_cast<parser_errc>(ev)) {
            case parser_errc::parser_fail_l0:
                return "parser fail at syntax level (l0)";
            case parser_errc::parser_fail_l1:
                return "parser fail at semantics level (l1)";
            case parser_errc::parser_fail_l2:
                return "parser fail at derived parsing level (l2)";
            default:
                return "unknown error";
        }
    }
};
const parser_err_category_t the_parser_err_cat{};
}  // namespace

std::error_code make_error_code(parser_errc ec) {
    return std::error_code(static_cast<int>(ec), the_parser_err_cat);
}

}  // namespace emailkit::imap_parser