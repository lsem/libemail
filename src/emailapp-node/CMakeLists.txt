# https://cmake.org/cmake/help/latest/module/FetchContent.html#populating-content-without-adding-it-to-the-build

include(FetchContent)

FetchContent_Declare(
    node_api_headers
    # can this be dynamic? in general it seems like we don't need to have this really specific
    # to the node we are going to use in our electron app. But in case we need to, then we should find a way
    # to decide on this dynamically during configure time depending on node version that is used.
    URL https://nodejs.org/dist/v18.18.0/node-v18.18.0-headers.tar.gz
)

FetchContent_MakeAvailable(node_api_headers)

set(CMAKE_JS_INC ${CMAKE_BINARY_DIR}/_deps/node_api_headers-src/include/node)

file(GLOB_RECURSE HPP_FILES "src/*.hpp")
file(GLOB_RECURSE CPP_FILES "src/*.cpp")

add_library(EmailApp SHARED ${HPP_FILES} ${CPP_FILES})
target_include_directories(EmailApp PUBLIC ${CMAKE_JS_INC})
target_compile_definitions(EmailApp PUBLIC -DNAPI_VERSION=4)
set_target_properties(EmailApp PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(EmailApp ${CMAKE_JS_LIB})

# if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
#   # Generate node.lib
#   execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
# endif()