
# emailkit
file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_tests\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_tests\\.cpp")

add_library(emailkit ${HPP_FILES} ${CPP_FILES})
add_library(emailkit::emailkit ALIAS emailkit)

target_link_libraries(emailkit PUBLIC async_kit::async_kit)

# set_target_properties(emailkit PROPERTIES CXX_STANDARD "17")

# emailkit-tests
file(GLOB_RECURSE TEST_HPP_FILES CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE TEST_CPP_FILES CONFIGURE_DEPENDS "*.cpp")
# list(FILTER TEST_CPP_FILES INCLUDE REGEX "(.*\\/src\\/sskitlib)(.*)(\\/tests\\/)(.*\\.cpp)")

# list(FILTER TEST_HEADER_FILES INCLUDE REGEX "(.*\\/src\\/sskitlib)(.*)(\\/tests\\/)(.*\\.h)")

message(STATUS "TEST_CPP_FILES: ${TEST_HPP_FILES}")
message(STATUS "TEST_CPP_FILES: ${TEST_CPP_FILES}")
