project(emailkit)

cmake_minimum_required(VERSION 3.5)

add_subdirectory(vendor/asio)
add_subdirectory(vendor/async_kit)
add_subdirectory(vendor/googletest)
add_subdirectory(src/emailkit)
