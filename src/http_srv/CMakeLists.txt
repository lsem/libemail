find_package(OpenSSL REQUIRED)

# http_srv
file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_tests\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_tests\\.cpp")


add_library(emailkit_http_srv ${HPP_FILES} ${CPP_FILES})
add_library(emailkit::http_srv ALIAS emailkit_http_srv)

target_include_directories(emailkit_http_srv PUBLIC include/)

target_link_libraries(emailkit_http_srv PUBLIC emailkit::global emailkit::log asio::asio OpenSSL::SSL)
