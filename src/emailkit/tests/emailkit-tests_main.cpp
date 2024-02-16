#include <gtest/gtest.h>
#include <emailkit/emailkit.hpp>
#include <emailkit/log.hpp>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    if (!emailkit::initialize()) {
        log_error("failed initializing emailkit");
        return 1;
    }

    return RUN_ALL_TESTS();
}
