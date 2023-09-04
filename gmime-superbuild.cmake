cmake_minimum_required(VERSION 3.16)

# https://www.scivision.dev/cmake-external-project-autotools/
# https://chromium.googlesource.com/external/github.com/grpc/grpc/+/HEAD/examples/cpp/helloworld/cmake_externalproject/CMakeLists.txt
# https://github.com/smfrpc/smf/blob/master/CMakeLists.txt.in
# https://stackoverflow.com/questions/55708589/how-to-pass-an-environment-variable-to-externalproject-add-configure-command
# https://gitlab.kitware.com/cmake/cmake/-/issues/15052
# https://cmake.org/pipermail/cmake/2015-February/059891.html
# https://cmake.org/cmake/help/latest/module/BundleUtilities.html

include(ExternalProject)
include(GNUInstallDirs)


set(compilers_override CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER})
set(superbuild_prefix ${CMAKE_BINARY_DIR}/install_prefix)
set(libdir lib)
set(libdir_abs_path ${superbuild_prefix}/${libdir})
set(pkg_config_path ${libdir_abs_path}/pkgconfig)

find_program(make_cmd NAMES gmake make mingw32-make REQUIRED)

########################################################################
# libFFI
ExternalProject_Add(libffi
    URL     https://github.com/libffi/libffi/releases/download/v3.4.4/libffi-3.4.4.tar.gz
    UPDATE_DISCONNECTED true
    # autoconf-based projects require libdir to be absolute
    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${superbuild_prefix}/lib/pkgconfig  <SOURCE_DIR>/configure --prefix=${superbuild_prefix} --libdir=${libdir_abs_path} ${compiler} ${rpath_ldflags} ${compilers_override}
    BUILD_COMMAND
        ${make_cmd} -j4
    INSTALL_COMMAND ${make_cmd} -j4 install
    # BUILD_BYPRODUCTS ${my_LIBRARY} # for ninja only
)

########################################################################
# Meson build system, needed for glib (https://mesonbuild.com/Getting-meson.html)
ExternalProject_Add(meson
    URL https://github.com/mesonbuild/meson/releases/download/1.2.1/meson-1.2.1.tar.gz
    UPDATE_DISCONNECTED true
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
set(meson_cmd "${CMAKE_BINARY_DIR}/meson-prefix/src/meson/meson.py")

########################################################################
# GLib
set(glib_libname ${CMAKE_SHARED_LIBRARY_PREFIX}glib-2.0${CMAKE_SHARED_LIBRARY_SUFFIX})
set(gobject_libname ${CMAKE_SHARED_LIBRARY_PREFIX}gobject-2.0${CMAKE_SHARED_LIBRARY_SUFFIX})
set(glib_include_directory1 ${superbuild_prefix}/include/glib-2.0/)
set(glib_include_directory2 ${libdir_abs_path}/glib-2.0/include)
ExternalProject_Add(external_glib
    GIT_REPOSITORY  https://github.com/GNOME/glib.git
    GIT_TAG         2.76.4
    UPDATE_DISCONNECTED true
    CONFIGURE_HANDLED_BY_BUILD true# cmake must support this even though we don't need it.
    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env ${compilers_override}
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
add_library(glib INTERFACE)
add_dependencies(glib external_glib)
target_link_directories(glib INTERFACE ${libdir_abs_path})
target_link_libraries(glib INTERFACE ${glib_libname} ${gobject_libname})
target_include_directories(glib INTERFACE ${glib_include_directory1} ${glib_include_directory2})
add_library(glib::glib ALIAS glib)

########################################################################
# GMime
set(gmime_libname ${CMAKE_SHARED_LIBRARY_PREFIX}gmime-3.0${CMAKE_SHARED_LIBRARY_SUFFIX})
set(gmime_include_directory ${superbuild_prefix}/include/gmime-3.0)
ExternalProject_Add(external_gmime
    URL
        https://github.com/lsem/gmime/releases/download/3.2.13-with-fixes/gmime-3.2.13--with-fixes.tar.gz
    UPDATE_DISCONNECTED
        true
    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${superbuild_prefix}/lib/pkgconfig LDFLAGS=-L${superbuild_prefix}/lib  <SOURCE_DIR>/configure --prefix=${superbuild_prefix} ${compilers_override}
    BUILD_COMMAND
        ${make_cmd} -j4
    INSTALL_COMMAND
        ${make_cmd} -j4 install
    DEPENDS
        external_glib
)
#https://github.com/cbm-fles/flesnet/blob/master/CMakeLists.txt
add_library(gmime INTERFACE)
add_dependencies(gmime external_gmime)
target_link_directories(gmime INTERFACE ${libdir_abs_path})
target_link_libraries(gmime INTERFACE  glib::glib ${gmime_libname})
target_include_directories(gmime INTERFACE ${gmime_include_directory})
add_library(gmime::gmime ALIAS gmime)


####################################################################################################
# # WIP: this is attempt to patch libraries.
# # The current problem is that we don't know what libraries we are linked against.
# # https://preshing.com/20170522/learn-cmakes-scripting-language-in-15-minutes/
# #
# # TODO: ideally we should not be patching this shit but have it done during compilation instead.
# set (patched_lib "gmime-3.0")
# set (patched_lib_fname "${CMAKE_SHARED_LIBRARY_PREFIX}${patched_lib}${CMAKE_SHARED_LIBRARY_SUFFIX}")
# set (patched_lib_abs_path "${superbuild_prefix}/lib/${patched_lib_fname}")

# set (libraries_list "gobject-2.0.0;gio-2.0.0;gmodule-2.0.0;gthread-2.0.0")
# foreach(lib ${libraries_list})
#    set (lib_fname "${CMAKE_SHARED_LIBRARY_PREFIX}${lib}${CMAKE_SHARED_LIBRARY_SUFFIX}")
#    set (lib_abs_path "${superbuild_prefix}/lib/${lib_fname}")
#    set (lib_rel_path "@rpath/${lib_fname}")
#    set (patched_lib_path )
#    # we change full path to relpath in library patched_lib_abs_path
#    set (install_name_change_cmd "install_name_tool -change ${lib_abs_path} ${lib_rel_path} ${patched_lib_abs_path}")
#    message("command: ${install_name_change_cmd}")
# endforeach()


# TODO:
# /usr/lib/libiconv.2.dylib
# /usr/local/opt/gettext/lib/libintl.8.dylib
# /opt/local/lib/libintl.8.dylib (compatibility version 10.0.0, current version 10.5.0)
# /opt/local/lib/libz.1.dylib (compatibility version 1.0.0, current version 1.2.11)
# /opt/local/lib/libiconv.2.dylib (compatibility version 9.0.0, current version 9.1.0)