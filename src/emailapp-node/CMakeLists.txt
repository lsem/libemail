add_definitions(-DNAPI_VERSION=4)
include_directories(${CMAKE_JS_INC})

file(GLOB_RECURSE HPP_FILES "src/*.hpp")
file(GLOB_RECURSE CPP_FILES "src/*.cpp")

add_library(EmailApp SHARED ${HPP_FILES} ${CPP_FILES})
set_target_properties(EmailApp PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(EmailApp ${CMAKE_JS_LIB})

# if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
#   # Generate node.lib
#   execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
# endif()