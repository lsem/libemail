add_library(emailkit_global include/emailkit/global.hpp src/global.cpp)
add_library(emailkit::global ALIAS emailkit_global)
target_include_directories(emailkit_global PRIVATE src PUBLIC include)

target_link_libraries(emailkit_global
    PUBLIC
    async_kit
    emailkit::log
    llvm_expected
    tl::expected)
