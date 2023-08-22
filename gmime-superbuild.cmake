# https://www.scivision.dev/cmake-external-project-autotools/
# https://chromium.googlesource.com/external/github.com/grpc/grpc/+/HEAD/examples/cpp/helloworld/cmake_externalproject/CMakeLists.txt
# https://github.com/smfrpc/smf/blob/master/CMakeLists.txt.in
# https://stackoverflow.com/questions/55708589/how-to-pass-an-environment-variable-to-externalproject-add-configure-command

include(ExternalProject)

set(superbuild_prefix ${CMAKE_BINARY_DIR}/install_prefix)
set(config_flags --prefix=${superbuild_prefix})
set(pkg_config_path ${superbuild_prefix}/lib/pkgconfig/)
find_program(make_cmd NAMES gmake make mingw32-make REQUIRED)

# first we need libffi
ExternalProject_Add(libffi
    URL     https://github.com/libffi/libffi/releases/download/v3.4.4/libffi-3.4.4.tar.gz
    UPDATE_DISCONNECTED true
    # CONFIGURE_HANDLED_BY_BUILD true for cmake and meson only
    CONFIGURE_COMMAND <SOURCE_DIR>/configure ${config_flags}
    BUILD_COMMAND ${make_cmd} -j4
    INSTALL_COMMAND ${make_cmd} -j4 install
    # BUILD_BYPRODUCTS ${my_LIBRARY} # for ninja only
)

# TODO: bootstrap meson also from internet.
# https://mesonbuild.com/Getting-meson.html
find_program(meson_cmd meson REQUIRED)
ExternalProject_Add(libglib
    GIT_REPOSITORY  https://github.com/GNOME/glib.git
    GIT_TAG         2.76.4
    UPDATE_DISCONNECTED true
    CONFIGURE_HANDLED_BY_BUILD true# cmake must support this even though we don't need it.
    CONFIGURE_COMMAND
        ${meson_cmd} setup
                    --prefix ${superbuild_prefix}
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
        libffi
)

ExternalProject_Add(libgmime
    URL
    https://github.com/lsem/gmime/releases/download/3.2.13-with-fixes/gmime-3.2.13--with-fixes.tar.gz
    UPDATE_DISCONNECTED
        true
    CONFIGURE_COMMAND
        # TODO: make env setting portable
        env PKG_CONFIG_PATH=${pkg_config_path} <SOURCE_DIR>/configure --prefix=${superbuild_prefix}
        BUILD_COMMAND ${make_cmd} -j4
        INSTALL_COMMAND ${make_cmd} -j4 install    
    DEPENDS
        libglib    
)
