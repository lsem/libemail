file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "src/*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE TEST_H_FILES  CONFIGURE_DEPENDS "tests/*.hpp")
file(GLOB_RECURSE TEST_CPP_FILES  CONFIGURE_DEPENDS "tests/*.cpp")

add_library(mailer_poc ${HPP_FILES} ${CPP_FILES})
add_library(mailer::poc ALIAS mailer_poc)
add_dependencies(mailer_poc external_glib external_gmime)
target_link_libraries(mailer_poc PUBLIC emailkit::emailkit emailkit::global asio::asio)
target_include_directories(mailer_poc PUBLIC include)

add_executable(mailer_poc_tests ${TEST_H_FILES} ${TEST_CPP_FILES})
target_link_libraries(mailer_poc_tests
      PRIVATE mailer_poc
              GTest::gtest
              GTest::gmock)
