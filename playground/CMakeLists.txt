# https://jip.dev/notes/cmake/
# This allows to make win32-like behavior that executable first tries to load all dependend dynamic libraries
# from the directory where exe is stored itself and only then system paths. This is very reasonable behavior
# for installations which suply dlls with executable for private use, probably patched.
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'$ORIGIN'")
set (CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set (CMAKE_INSTALL_RPATH "$ORIGIN")

add_executable(emailkit_playground emailkit_playground_main.cpp)
add_dependencies(emailkit_playground external_glib external_gmime)
target_link_libraries(emailkit_playground
    PRIVATE
        emailkit::emailkit
        emailkit::http_srv
        pthread
        folly::uri
        b64::b64
        gmime::gmime)

target_compile_options(emailkit_playground PRIVATE "-fsanitize=address")
target_link_options(emailkit_playground PRIVATE "-fsanitize=address")
