find_package(OpenSSL REQUIRED)

# emailkit
file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "src/*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "src/*.cpp")

set (IMAP_PARSER_SRC "src/imap_parser_apg_impl.h" "src/imap_parser_apg_impl.c")

add_library(emailkit ${HPP_FILES} ${CPP_FILES} ${IMAP_PARSER_SRC})
add_library(emailkit::emailkit ALIAS emailkit)

target_include_directories(emailkit PUBLIC include/)

# Lets try to iterate filess in include directory
file(GLOB PUBLIC_HPP_FILES CONFIGURE_DEPENDS "include/emailkit/*.hpp")
set(PUBLIC_HPP_FILES_IN_SRC "")
message("PUBLIC_HPP_FILES: ${PUBLIC_HPP_FILES}")
foreach(file_path ${PUBLIC_HPP_FILES})
  cmake_path(GET file_path FILENAME file_name)
  set(header_path_in_src src/${file_name})
  message("file: " ${file_name} -> ${header_path_in_src})
  list(APPEND PUBLIC_HPP_FILES_IN_SRC ${header_path_in_src})
endforeach()
message("Resulting list: ${PUBLIC_HPP_FILES_IN_SRC}")

install(FILES ${PUBLIC_HPP_FILES_IN_SRC} DESTINATION include/emailkit)

add_compile_definitions(APG_DEBUG APG_TRACE)

target_link_libraries(emailkit 
  PUBLIC
        async_kit
        emailkit::log
        emailkit::http_srv
        folly::uri
        asio::asio
        OpenSSL::SSL
        rapidjson::rapidjson
        b64::b64
        apg::library
        apg::utilities
        ricab::scope_guard
        skeeto::utf7
        gmime::gmime)

target_compile_options(emailkit PUBLIC "-fsanitize=address")
target_link_options(emailkit PUBLIC "-fsanitize=address")
        


##############################################################################
# emailkit-tests
file(GLOB_RECURSE TESTS_HPP_FILES  CONFIGURE_DEPENDS "tests/*.hpp")
file(GLOB_RECURSE TESTS_CPP_FILES  CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(emailkit-tests ${TESTS_HPP_FILES} ${TESTS_CPP_FILES})
target_link_libraries(emailkit-tests
  PRIVATE        
        emailkit::emailkit
        GTest::gtest
        GTest::gmock)

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
configure_file(tests/rfc822_gmail_msg.dat ${CMAKE_BINARY_DIR}/)
configure_file(tests/rfc822_gmail_headers_massive_pack.dat ${CMAKE_BINARY_DIR}/)
configure_file(tests/rfc822_gmail_msg__utf8_subject.dat ${CMAKE_BINARY_DIR}/)
configure_file(tests/gmail_autoreply_test.dat ${CMAKE_BINARY_DIR}/)

