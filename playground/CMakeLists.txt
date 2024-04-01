# We want to make our executables relocatable so that they have all theor dependencies with 
#
# https://jip.dev/notes/cmake/
# This allows to make win32-like behavior that executable first tries to load all dependend dynamic libraries
# from the directory where exe is stored itself and only then system paths. This is very reasonable behavior
# for installations which suply dlls with executable for private use, probably patched.
# https://shotcut.org/notes/macosdev/
# https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling
# https://nehckl0.medium.com/creating-relocatable-linux-executables-by-setting-rpath-with-origin-45de573a2e98
# https://matthew-brett.github.io/docosx/mac_runtime_link.html

if(APPLE)
    message("debug: APPLE")
    #set  (CMAKE_INSTALL_RPATH = @executable_path/../Frameworks)
    set (CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set (CMAKE_INSTALL_RPATH "@executable_path")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'@executable_path'")
    set (CMAKE_MACOSX_RPATH 1)
elseif(UNIX)
    # message("debug: UNIX")
    # set (CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    # set (CMAKE_INSTALL_RPATH "$ORIGIN")
    # #set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    # set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
    # set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'$ORIGIN'")        
endif()


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
