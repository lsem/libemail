#include <emailkit/global.hpp>
#include <emailkit/log.hpp>

/*static*/ void emailkit_log_fns::print_error_line(const char* msg) {
    log_error("{}", msg);
}
