#include "emailkit/emailkit.hpp"
#include "imap_parser.hpp"

namespace emailkit {
expected<void> initialize() {
    return imap_parser::initialize();
}
expected<void> finalize() {
    return imap_parser::finalize();
}
}  // namespace emailkit
