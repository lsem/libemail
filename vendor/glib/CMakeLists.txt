# https://www.scivision.dev/cmake-external-project-autotools/

include(ExternalProject)

set(config_flags --install-prefix="${CMAKE_BINARY_DIR}/install_prefix")

find_program(MAKE_EXECUTABLE NAMES gmake make mingw32-make REQUIRED)

# set(my_LIBRARY ${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}my${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(glib
    GIT_REPOSITORY https://github.com/GNOME/glib.git 
    GIT_TAG 2.77.2
    UPDATE_DISCONNECTED true
    CONFIGURE_HANDLED_BY_BUILD true
    CONFIGURE_COMMAND <SOURCE_DIR>/configure ${config_flags}
    BUILD_COMMAND ${MAKE_EXECUTABLE} -j4
    INSTALL_COMMAND ${MAKE_EXECUTABLE} -j4 install
    # TEST_COMMAND ""
    # BUILD_BYPRODUCTS ${my_LIBRARY} # for ninja only
)

add_library(glib::glib INTERFACE IMPORTED GLOBAL)

meson setup builddir -Dprefix=/tmp/install --pkg-config-path "/tmp/install/lib/x86_64-linux-gnu/pkgconfig/"