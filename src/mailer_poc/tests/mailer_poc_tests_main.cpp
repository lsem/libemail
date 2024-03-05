#include <gtest/gtest.h>


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // if (!emailkit::initialize()) {
    //     log_error("failed initializing emailkit");
    //     return 1;
    // }

    return RUN_ALL_TESTS();
}
