find_package(OpenSSL REQUIRED)

# emailkit
file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_tests\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_tests\\.cpp")

set (IMAP_PARSER_SRC "src/imap_parser.h" "src/imap_parser.c")

add_library(emailkit ${HPP_FILES} ${CPP_FILES} ${IMAP_PARSER_SRC})
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
        b64::b64
        apg::library)

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

# Our tests require some certificate for running test server,we use self-signed one:
# To generate new keys use:
# openssl genrsa -out privkey.pem 2048
# openssl req -new -key privkey.pem  -out certreq.csr
# openssl x509 -req -days 3650 -in certreq.csr -signkey privkey.pem -out newcert.pem
# TODO: consider generating this on the fly since we have dependency on OpenSSL here.
configure_file(tests/newcert.pem ${CMAKE_BINARY_DIR}/)
configure_file(tests/privkey.pem ${CMAKE_BINARY_DIR}/)
