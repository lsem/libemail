file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "src/*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "src/*.cpp")

add_executable(abnf-helper ${CPP_FILES})
target_link_libraries(abnf-helper PUBLIC emailkit emailkit::log)
