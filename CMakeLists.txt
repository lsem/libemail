project(emailkit)

cmake_minimum_required(VERSION 3.5)

set (CMAKE_CXX_STANDARD 20)


# add_compile_options("-fcolor-diagnostics")
add_compile_options("-fdiagnostics-color=always")

add_subdirectory(vendor/googletest)
add_subdirectory(vendor/fmt)
add_subdirectory(vendor/asio)
add_subdirectory(vendor/async_kit)
add_subdirectory(vendor/folly_uri)
add_subdirectory(vendor/rapidjson)
add_subdirectory(vendor/apg-7.0/apg)
add_subdirectory(vendor/scope_guard)
add_subdirectory(vendor/libb64)
add_subdirectory(vendor/utf-7)
add_subdirectory(vendor/expected)
include(gmime-superbuild.cmake)

add_subdirectory(src/global)
add_subdirectory(src/log)
add_subdirectory(src/emailkit)
add_subdirectory(src/emailapp-core)
add_subdirectory(src/emailapp-node)
add_subdirectory(src/tools/bin2cpplit)
add_subdirectory(src/tools/abnf-helper)
add_subdirectory(playground)
add_subdirectory(src/http_srv)

