file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "src/*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "src/*.cpp")

add_library(mailer_poc ${HPP_FILES} ${CPP_FILES})
add_library(mailer::poc ALIAS mailer_poc)
add_dependencies(mailer_poc external_glib external_gmime)
target_link_libraries(mailer_poc PUBLIC emailkit::emailkit emailkit::global asio::asio)
target_include_directories(mailer_poc PUBLIC include)
