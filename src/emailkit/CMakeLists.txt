find_package(OpenSSL REQUIRED)

# emailkit
file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_tests\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_tests\\.cpp")

add_library(emailkit ${HPP_FILES} ${CPP_FILES})
add_library(emailkit::emailkit ALIAS emailkit)

target_include_directories(emailkit PUBLIC include/)

target_link_libraries(emailkit 
    PUBLIC
        emailkit::global
        emailkit::log
        emailkit::http_srv
        folly::uri
        asio::asio
        OpenSSL::SSL
        rapidjson::rapidjson
        b64::b64)

##############################################################################
# emailkit-tests
file(GLOB_RECURSE TESTS_HPP_FILES  CONFIGURE_DEPENDS "tests/*.hpp")
file(GLOB_RECURSE TESTS_CPP_FILES  CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(emailkit-tests ${TESTS_HPP_FILES} ${TESTS_CPP_FILES})
target_link_libraries(emailkit-tests
    PRIVATE
        emailkit::emailkit
        GTest::gtest
        GTest::gmock
        GTest::gtest_main)

target_compile_options(emailkit-tests PRIVATE "-fsanitize=address")
target_link_options(emailkit-tests PRIVATE "-fsanitize=address")
