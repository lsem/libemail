file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "*.cpp")

add_library(emailkit_log ${HPP_FILES} ${CPP_FILES})
add_library(emailkit::log ALIAS emailkit_log)

target_include_directories(emailkit_log PUBLIC include/)
target_link_libraries(emailkit_log PUBLIC fmt::fmt)