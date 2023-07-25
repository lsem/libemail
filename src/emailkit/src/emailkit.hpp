#pragma once
#include <async_kit/async_callback.hpp>

namespace emailkit {
class emailkit {
    public:
        void async_method(async_callback<void> cb) {
            cb(std::error_code());
        }
};
} // namespace emailkit