file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "src/*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE TEST_H_FILES  CONFIGURE_DEPENDS "tests/*.hpp")
file(GLOB_RECURSE TEST_CPP_FILES  CONFIGURE_DEPENDS "tests/*.cpp")

add_library(mailer_poc SHARED ${HPP_FILES} ${CPP_FILES})
add_library(mailer::poc ALIAS mailer_poc)
add_dependencies(mailer_poc external_glib external_gmime)
target_link_libraries(mailer_poc PUBLIC emailkit::emailkit asio::asio)
target_include_directories(mailer_poc PUBLIC include)

add_executable(mailer_poc_tests ${TEST_H_FILES} ${TEST_CPP_FILES})
target_link_libraries(mailer_poc_tests
      PRIVATE mailer_poc
              GTest::gtest
              GTest::gmock)

install(TARGETS mailer_poc DESTINATION lib)

file(GLOB PUBLIC_HPP_FILES CONFIGURE_DEPENDS "include/*.hpp")
set(PUBLIC_HPP_FILES_IN_SRC "")
message("PUBLIC_HPP_FILES: ${PUBLIC_HPP_FILES}")
foreach(file_path ${PUBLIC_HPP_FILES})
  cmake_path(GET file_path FILENAME file_name)
  set(header_path_in_src src/${file_name})
  message("file: " ${file_name} -> ${header_path_in_src})
  list(APPEND PUBLIC_HPP_FILES_IN_SRC ${header_path_in_src})
endforeach()
message("Resulting list: ${PUBLIC_HPP_FILES_IN_SRC}")

# TODO: employ proper prefix!
install(FILES ${PUBLIC_HPP_FILES_IN_SRC} DESTINATION include/)
	    
