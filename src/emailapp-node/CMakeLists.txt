# https://cmake.org/cmake/help/latest/module/FetchContent.html#populating-content-without-adding-it-to-the-build

include(FetchContent)

set(node_cmd "node")
set(node_args "--version")

if (NODE_JS_BINARY)
    message("using NODE_JS_BINARY provided as configure option: ${NODE_JS_BINARY}")
    set(node_cmd ${NODE_JS_BINARY})
endif()

execute_process(COMMAND ${node_cmd} ${node_args}
    RESULT_VARIABLE node_version_exit_code
    OUTPUT_VARIABLE node_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)

if (node_version_exit_code AND NOT node_version_exit_code EQUAL 0)
    message(FATAL_ERROR "nodejs has not been found in path. To specify nodejs binary location use -DNODE_JS_BINARY=<path-to-node-exe> variable or exclude emailapp-node extension use -DEXCLUDE_NODE_EXTENSION=true")
endif()

message("node version determined as: ${node_version}")

set(node_headers_url "https://nodejs.org/dist/${node_version}/node-${node_version}-headers.tar.gz")
FetchContent_Declare(node_api_headers URL ${node_headers_url})
FetchContent_MakeAvailable(node_api_headers)

set(CMAKE_JS_INC ${CMAKE_BINARY_DIR}/_deps/node_api_headers-src/include/node)

file(GLOB_RECURSE HPP_FILES "src/*.hpp")
file(GLOB_RECURSE CPP_FILES "src/*.cpp")

add_library(EmailApp SHARED ${HPP_FILES} ${CPP_FILES})
target_include_directories(EmailApp PUBLIC ${CMAKE_JS_INC})
target_compile_definitions(EmailApp PUBLIC -DNAPI_VERSION=4)
target_link_libraries(EmailApp emailapp::core ${CMAKE_JS_LIB})
set_target_properties(EmailApp PROPERTIES PREFIX "" SUFFIX ".node")

# if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
#   # Generate node.lib
#   execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
# endif()
