#pragma once

#include <emailkit/global.hpp>

namespace emailkit {
expected<void> initialize();
expected<void> finalize();
}  // namespace emailkit
