#include <emailkit/global.hpp>

#include <sstream>

namespace details {
std::string to_string(const llvm::Error& e) {
    std::stringstream ss;
    ss << e;
    return ss.str();
}
}  // namespace details
