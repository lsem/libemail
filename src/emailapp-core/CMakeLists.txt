file(GLOB_RECURSE HPP_FILES  CONFIGURE_DEPENDS "src/*.hpp")
file(GLOB_RECURSE CPP_FILES  CONFIGURE_DEPENDS "src/*.cpp")

add_library(emailapp_core ${HPP_FILES} ${CPP_FILES})
add_library(emailapp::core ALIAS emailapp_core)
add_dependencies(emailapp_core external_glib external_gmime)
target_link_libraries(emailapp_core PUBLIC emailkit::emailkit asio::asio)
target_include_directories(emailapp_core PUBLIC include)

