cmake_minimum_required(VERSION 3.16)

# https://www.scivision.dev/cmake-external-project-autotools/
# https://chromium.googlesource.com/external/github.com/grpc/grpc/+/HEAD/examples/cpp/helloworld/cmake_externalproject/CMakeLists.txt
# https://github.com/smfrpc/smf/blob/master/CMakeLists.txt.in
# https://stackoverflow.com/questions/55708589/how-to-pass-an-environment-variable-to-externalproject-add-configure-command

include(ExternalProject)
include(GNUInstallDirs)

set(superbuild_prefix ${CMAKE_BINARY_DIR}/install_prefix)
set(libdir lib)
set(libdir_abs_path ${superbuild_prefix}/${libdir})
set(pkg_config_path ${libdir_abs_path}/pkgconfig)

find_program(make_cmd NAMES gmake make mingw32-make REQUIRED)

# libFFI
ExternalProject_Add(libffi
    URL     https://github.com/libffi/libffi/releases/download/v3.4.4/libffi-3.4.4.tar.gz
    UPDATE_DISCONNECTED true
    # autoconf-based projects require libdir to be absolute
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${superbuild_prefix} --libdir=${libdir_abs_path}
    BUILD_COMMAND ${make_cmd} -j4
    INSTALL_COMMAND ${make_cmd} -j4 install
    # BUILD_BYPRODUCTS ${my_LIBRARY} # for ninja only
)

# Meson build system, needed for glib (https://mesonbuild.com/Getting-meson.html)
ExternalProject_Add(meson
    URL https://github.com/mesonbuild/meson/releases/download/1.2.1/meson-1.2.1.tar.gz
    UPDATE_DISCONNECTED true
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
set(meson_cmd "${CMAKE_BINARY_DIR}/meson-prefix/src/meson/meson.py")

ExternalProject_Add(libglib
    GIT_REPOSITORY  https://github.com/GNOME/glib.git
    GIT_TAG         2.76.4
    UPDATE_DISCONNECTED true
    CONFIGURE_HANDLED_BY_BUILD true# cmake must support this even though we don't need it.
    CONFIGURE_COMMAND
        ${meson_cmd} setup
                    --prefix ${superbuild_prefix}
                    --libdir ${libdir}
                    --pkg-config-path ${pkg_config_path}
                    -Dgtk_doc=false
                    -Dman=false
                    -Dxattr=false
                    -Dselinux=disabled
                    -Ddtrace=false
                    -Db_coverage=false
                    -Dtests=false # tests require ICU
                    -Dinstalled_tests=false
                    _build <SOURCE_DIR>
    BUILD_COMMAND
        ${meson_cmd} compile -C _build
    INSTALL_COMMAND
        ${meson_cmd} install -C _build    
    DEPENDS
        libffi meson
)

ExternalProject_Add(libgmime
    URL
    https://github.com/lsem/gmime/releases/download/3.2.13-with-fixes/gmime-3.2.13--with-fixes.tar.gz
    UPDATE_DISCONNECTED
        true
    CONFIGURE_COMMAND
        # TODO: make env setting portable
        env PKG_CONFIG_PATH=${pkg_config_path} <SOURCE_DIR>/configure --prefix=${superbuild_prefix} --libdir=${libdir_abs_path}
        BUILD_COMMAND ${make_cmd} -j4
        INSTALL_COMMAND ${make_cmd} -j4 install    
    DEPENDS
        libglib    
)
