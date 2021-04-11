# NMS utils
# Copyright (C) 2020-2021 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

# This happens to be my first CMake project, so it's undoubtably wonky

# Don't actually know the minimum version this'll work with, 3.18 is what I have
cmake_minimum_required (VERSION 3.18)

list (PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include (NMSBuild)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project (NMS VERSION 1.0 LANGUAGES CXX)
  set (CMAKE_CXX_STANDARD 20)
endif ()

set (NMS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set (NMS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

include (CheckLibraryExists)
include (CheckIncludeFileCXX)

set (DFLT_CHECKING ON)
if (CMAKE_BUILD_TYPE MATCHES "^Rel")
  set (DFLT_CHECKING OFF)
endif ()
option (NMS_CHECKING "Enable checking" ${DFLT_CHECKING})
message (NOTICE "NMS checking:${NMS_CHECKING}")
option (NMS_BACKTRACE "Enable backtrace on crash" ON)
message (NOTICE "NMS backtrace:${NMS_BACKTRACE}")

if (NMS_BACKTRACE)
  check_library_exists (bfd bfd_init "/usr/lib" HAVE_BFD)
  check_include_file_cxx ("ucontext.h" HAVE_UCONTEXT)
endif ()

# Intentionally containing project root
file (RELATIVE_PATH BIN2SRC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})

configure_file (nmscfg.h.in nmscfg.h ESCAPE_QUOTES @ONLY)
configure_file (gdbinit.in gdbinit ESCAPE_QUOTES @ONLY)

include_directories (${NMS_BINARY_DIR} ${NMS_INCLUDE_DIR})

# The user visible library
add_library (nms libnms/fatal.cc libnms/option.cc)
# The signal handler needs a frame pointer
set_source_files_properties (libnms/fatal.cc
  PROPERTIES COMPILE_OPTIONS -fno-omit-frame-pointer)
if (CMAKE_BUILD_TYPE MATCHES "^Rel")
  # These care about size more than speed
  set_source_files_properties (libnms/fatal.cc
    PROPERTIES COMPILE_OPTIONS -Os)
endif ()

if (HAVE_BFD)
  target_link_libraries (nms PRIVATE bfd)
endif ()

# exports
set (NMS_INCLUDE_DIR ${NMS_INCLUDE_DIR} PARENT_SCOPE)
set (NMS_BINARY_DIR ${NMS_BINARY_DIR} PARENT_SCOPE)
