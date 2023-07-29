project(emailkit)

cmake_minimum_required(VERSION 3.5)

set (CMAKE_CXX_STANDARD 17)

# add_compile_options("-fcolor-diagnostics")
add_compile_options("-fdiagnostics-color=always")

add_subdirectory(vendor/fmt)
add_subdirectory(vendor/asio)
add_subdirectory(vendor/async_kit)
add_subdirectory(vendor/googletest)
add_subdirectory(src/global)
add_subdirectory(src/log)
add_subdirectory(src/emailkit)
add_subdirectory(playground)
