# Try to find fuse (devel)
# Once done, this will define
#
# FUSE_FOUND        - system has fuse
# FUSE_INCLUDE_DIRS - fuse include directories
# FUSE_LIBRARIES    - libraries need to use fuse
# FUSE_MOUNT_VERSION - major version reported by fusermount 
#
# and the following imported target
# FUSE::FUSE
message("====== FuseFind3 ======")
find_package(PkgConfig)
pkg_check_modules(PC_fuse QUIET fuse3)
set(FUSE_VERSION ${PC_fuse_VERSION})
message("FUSE_VERSION=${FUSE_VERSION}")
message("FUSE_ROOT=${FUSE_ROOT}")

find_path(FUSE_INCLUDE_DIR
  NAMES fuse_lowlevel.h
  HINTS ${FUSE_ROOT} ${PC_fuse_INCLUDEDIR} ${PC_fuse_INCLUDE_DIRS}
  PATH_SUFFIXES include include/osxfuse)

if(MacOSX)
  find_library(FUSE_LIBRARY
    NAMES osxfuse3
    HINTS ${FUSE_ROOT} ${PC_fuse_LIBDIR} ${PC_fuse_LIBRARY_DIRS}
    PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR})
else()
  find_library(FUSE_LIBRARY
    NAMES fuse3
    HINTS ${FUSE_ROOT} ${PC_fuse_LIBDIR} ${PC_fuse_LIBRARY_DIRS}
    PATH_SUFFIXES$ ${CMAKE_INSTALL_LIBDIR})


  execute_process(
    COMMAND sh -c "fusermount --version | cut -d ' ' -f 3 | cut -d '.' -f 1,2 | sed s/'\\.'//g"
    OUTPUT_VARIABLE FUSE_MOUNT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RETC)
  if(NOT ("${RETC}" STREQUAL "0") )
    set(${FUSE_MOUNT_VERSION} "" PARENT_SCOPE)
  endif()

  message(STATUS "Setting FUSE_MOUNT_VERSION: ${FUSE_MOUNT_VERSION}")  
endif()

message("FUSE_INCLUDE_DIR=${FUSE_INCLUDE_DIR}")
message("FUSE_LIBRARY=${FUSE_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(fuse3
  REQUIRED_VARS FUSE_LIBRARY FUSE_INCLUDE_DIR
  VERSION_VAR FUSE_VERSION)

set(FUSE_FOUND ${FUSE3_FOUND})
message("FUSE_FOUND=${FUSE_FOUND}")
message("FUSE3_FOUND=${FUSE3_FOUND}")

if (FUSE_FOUND AND NOT TARGET FUSE::FUSE)
  mark_as_advanced(FUSE_INCLUDE_DIR FUSE_LIBRARY)
  add_library(FUSE::FUSE UNKNOWN IMPORTED)
  set_target_properties(FUSE::FUSE PROPERTIES
    IMPORTED_LOCATION "${FUSE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FUSE_INCLUDE_DIR}")
endif()

set(FUSE_INCLUDE_DIRS ${FUSE_INCLUDE_DIR})
set(FUSE_LIBRARIES ${FUSE_LIBRARY})
unset(FUSE_INCLDUE_DIR)
unset(FUSE_LIBRARY)
message("====== FuseFind3 ======")
