cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(mergerfs LANGUAGES CXX)

include(CMakeDependentOption)
include(GNUInstallDirs)

if(NOT DEFINED ENABLE_XATTR)
    set(ENABLE_XATTR "AUTO")
endif()
# we could have used option(ENABLE_XATTR) here, but it doesn't allow for AUTO behavior
set(ENABLE_XATTR "${ENABLE_XATTR}" CACHE STRING "Build with xattr support (AUTO, YES, NO)")
CMAKE_DEPENDENT_OPTION(WITH_XATTR "Include dir with xattr headers"
    OFF "ENABLE_XATTR"
    OFF)
option(WITH_INTERNAL_FUSE "Use bundled (YES) or system (NO) libfuse" "YES")

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING)
endif()

file(GLOB mergerfs_sources src/*.cpp src/*.icpp src/*.hpp src/*.h)
set(version_hpp "${CMAKE_CURRENT_BINARY_DIR}/version.hpp")
list(APPEND mergerfs_sources ${version_hpp})
add_executable(mergerfs ${mergerfs_sources})
target_include_directories(mergerfs PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

set(GIT2DEBCL "${CMAKE_CURRENT_SOURCE_DIR}/tools/git2debcl")

find_program(PANDOC pandoc)
if(NOT PANDOC)
    message(WARNING "pandoc does not appear available: manpage won't be buildable")
endif()

if(NOT DEFINED ${MERGERFS_VERSION})
    set(git_toplevel_dir)
    find_program(GIT git)
    if(GIT)
        execute_process(
            COMMAND ${GIT} rev-parse --show-toplevel
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE git_toplevel_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(git_toplevel_dir)
            file(TO_CMAKE_PATH "${git_toplevel_dir}" git_toplevel_dir)
        endif()
    endif()
    if(git_toplevel_dir STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        # we're in a git worktree, and not in a super-project git tree
        execute_process(
            COMMAND ${GIT} describe --always --tags --dirty
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE MERGERFS_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" MERGERFS_VERSION)
    else()
        message(FATAL_ERROR "mergerfs version can't be determined. Use release tarball or run from within mergerfs git repository")
    endif()
endif()

# XXX: might get stale
# after MERGERFS_VERSION is defined
configure_file(src/version.hpp.in "${version_hpp}" @ONLY)

if(ENABLE_XATTR) # including "AUTO"
    if(WITH_XATTR)
        find_path(XATTR_INCLUDE_DIR attr/xattr.h PATHS "${WITH_XATTR}")
    else()
        find_path(XATTR_INCLUDE_DIR attr/xattr.h)
    endif()
    if(NOT XATTR_INCLUDE_DIR)
        if(ENABLE_XATTR EQUAL "AUTO")
            set(msg_level "WARNING")
        else()
            # xattr is explicitly requested, but is not available
            set(msg_level "FATAL_ERROR")
        endif()
        message("${msg_level}" "xattr not available: disabling")
    endif()
else()
    set(XATTR_INCLUDE_DIR NO)
endif()
if(XATTR_INCLUDE_DIR)
    target_include_directories(mergerfs PRIVATE "${XATTR_INCLUDE_DIR}")
else()
    target_compile_definitions(mergerfs PRIVATE "-DWITHOUT_XATTR")
endif()

if(WITH_INTERNAL_FUSE)
    include(ExternalProject)
    ExternalProject_Add(libfuse_internal
        URL "${CMAKE_CURRENT_SOURCE_DIR}/libfuse"
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory m4
        COMMAND autoreconf --force --install
        COMMAND ./configure CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} --enable-lib --disable-util --disable-example
        BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
        INSTALL_COMMAND true
        BUILD_IN_SOURCE 1
    )
    ExternalProject_Get_Property(libfuse_internal SOURCE_DIR)
    ExternalProject_Get_Property(libfuse_internal BINARY_DIR)
    add_library(libfuse STATIC IMPORTED)
    set_property(TARGET libfuse PROPERTY IMPORTED_LOCATION "${BINARY_DIR}/lib/.libs/libfuse.a")

    add_dependencies(mergerfs libfuse_internal)
    target_include_directories(mergerfs PRIVATE "${SOURCE_DIR}/include")
    target_compile_definitions(mergerfs PRIVATE "-D_FILE_OFFSET_BITS=64")
    target_link_libraries(mergerfs PRIVATE libfuse dl)
    set_property(TARGET mergerfs APPEND PROPERTY LINK_FLAGS "-pthread")
else()
    set(EXTERNAL_FUSE_MIN_REQ 2.9.7)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(FUSE REQUIRED "fuse >= ${EXTERNAL_FUSE_MIN_REQ}")
    target_include_directories(mergerfs PRIVATE "${FUSE_INCLUDE_DIRS}")
    target_compile_options(mergerfs PRIVATE "${FUSE_CFLAGS_OTHER}")
    target_link_libraries(mergerfs PRIVATE "${FUSE_LIBRARIES}")
    set_property(TARGET mergerfs APPEND PROPERTY LINK_FLAGS "${FUSE_LDFLAGS_OTHER}")
    link_directories("${FUSE_LIBRARY_DIRS}")
endif()

set(UGID_USE_RWLOCK 0)

target_compile_definitions(mergerfs PRIVATE
    "-DFUSE_USE_VERSION=29"
    "-DUGID_USE_RWLOCK=${UGID_USE_RWLOCK}"
)

target_compile_options(mergerfs PRIVATE "-Wall" "-Wno-unused-result")

add_custom_command(OUTPUT mount.mergerfs
    COMMAND ${CMAKE_COMMAND} -E create_symlink mergerfs mount.mergerfs
    DEPENDS mergerfs
    VERBATIM
)
add_custom_target(create-mount.mergerfs ALL
    DEPENDS mount.mergerfs
)


install(TARGETS mergerfs DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mount.mergerfs" DESTINATION "${CMAKE_INSTALL_BINDIR}")

set(manpage "${CMAKE_CURRENT_SOURCE_DIR}/man/mergerfs.1")

if(PANDOC)
    set(manpage "${CMAKE_CURRENT_BINARY_DIR}/mergerfs.1")
    add_custom_command(OUTPUT "${manpage}"
        COMMAND ${PANDOC} -s -t man -o "${manpage}" "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        DEPENDS README.md
        VERBATIM
    )
endif()
install(
    FILES "${manpage}"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
)
add_custom_target(man ALL DEPENDS "${manpage}")


if(GIT)
    add_custom_target(changelog
        COMMAND ${GIT2DEBCL} --name ${PROJECT_NAME} > ${CMAKE_CURRENT_BINARY_DIR}/changelog
        BYPRODUCTS changelog
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(authors
        COMMAND ${GIT} log --format='%aN <%aE>' | sort -fu > ${CMAKE_CURRENT_BINARY_DIR}/AUTHORS
        BYPRODUCTS AUTHORS
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()

if(EXISTS /usr/bin/apt-get)
    add_custom_target(install-build-pkgs
        COMMAND apt-get -qy update
        COMMAND apt-get -qy --no-install-suggests --no-install-recommends --force-yes install build-essential git g++ debhelper libattr1-dev python automake libtool lsb-release
    )
elseif(EXISTS /usr/bin/dnf)
    add_custom_target(install-build-pkgs
        COMMAND dnf -y update
        COMMAND dnf -y install git rpm-build libattr-devel gcc-c++ make which python automake libtool gettext-devel
    )
elseif(EXISTS /usr/bin/yum)
    add_custom_target(install-build-pkgs
        COMMAND yum -y update
        COMMAND yum -y install git rpm-build libattr-devel gcc-c++ make which python automake libtool gettext-devel
    )
endif()





set(CPACK_GENERATOR "DEB;RPM")

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${MERGERFS_VERSION}")
set(CPACK_PACKAGE_CONTACT "Antonio SJ Musumeci <trapexit@spawn.link>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "another FUSE union filesystem")
set(package_long_description
    "mergerfs is similar to mhddfs, unionfs, and aufs.
    Like mhddfs in that it too uses FUSE.
    Like aufs in that it provides multiple policies for how to handle behavior."
)
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "\\$\{shlibs\:Depends}, \\$\{misc\:Depends}, fuse [linux-any] | fuse4bsd [kfreebsd-any]")
set(CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS "debhelper (>= 8.0.0), libattr1-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://github.com/trapexit/mergerfs")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}
    ${package_long_description}"
)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "ON")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION "ON")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/docs;${CMAKE_CURRENT_SOURCE_DIR}/debian/copyright;${CMAKE_CURRENT_BINARY_DIR}/changelog"
)

set(CPACK_RPM_PACKAGE_RELEASE "1%{?dist}")
set(CPACK_RPM_PACKAGE_LICENSE "ISC")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_URL "http://github.com/trapexit/mergerfs")
set(CPACK_RPM_PACKAGE_REQUIRES "fuse")
set(CPACK_RPM_PACKAGE_SUMMARY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${package_long_description}")
# TODO: CPACK_RPM_CHANGELOG_FILE

set(CPACK_SOURCE_GENERATOR "TGZ")

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${MERGERFS_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/.git/")
# CPACK_SOURCE_INSTALLED_DIRECTORIES
# custom_target instead of cpack source generator?

include(CPack)
