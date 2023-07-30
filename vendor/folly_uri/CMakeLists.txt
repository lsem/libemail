cmake_minimum_required(VERSION 3.5)

set (CMAKE_CXX_STANDARD 17)

add_library(folly_uri include/folly/folly_uri.hpp src/folly_uri.cpp)
add_library(folly::uri ALIAS folly_uri)

target_include_directories(folly_uri PUBLIC include)
target_link_libraries(folly_uri PUBLIC llvm_expected)