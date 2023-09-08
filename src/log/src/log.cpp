#include "log.hpp"

/*extern*/ log_level_t g_current_level = log_level_t::info;
/*extern*/ std::chrono::steady_clock::time_point g_local_epooch = std::chrono::steady_clock::now();