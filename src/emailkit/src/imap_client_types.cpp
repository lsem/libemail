#include "imap_client_types.hpp"

namespace emailkit::imap_client::types {

namespace {
struct imap_errors_cat_t : std::error_category {
   public:
    const char* name() const noexcept override { return "imap_errors"; }
    std::string message(int ev) const override {
        switch (static_cast<imap_errors>(ev)) {
            case imap_errors::imap_bad:
                return "imap_bad";
            case imap_errors::imap_no:
                return "imap_no";
            default:
                return "unrecognized error";
        }
    }
};

const imap_errors_cat_t the_imap_errors_category;
}  // namespace

std::error_code make_error_code(imap_errors e) {
    return {static_cast<int>(e), the_imap_errors_category};
}

}  // namespace emailkit::imap_client::types