# NMS utils
# Copyright (C) 2020-2021 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

# This happens to be my first CMake project, so it's undoubtably wonky

# Don't actually know the minimum version this'll work with, 3.18 is what I have
cmake_minimum_required (VERSION 3.18)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project (NMS VERSION 1.0 LANGUAGES CXX)
  message ("${CMAKE_BUILD_TYPE} build")
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
  check_library_exists (iberty cplus_demangle "/usr/lib" HAVE_DEMANGLE)
  check_include_file_cxx ("ucontext.h" HAVE_UCONTEXT)
endif ()

# Intentionally containing project root
file (RELATIVE_PATH BIN2SRC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})

configure_file (config.h.in config.h ESCAPE_QUOTES @ONLY)
configure_file (gdbinit.in gdbinit ESCAPE_QUOTES @ONLY)

include_directories (${NMS_BINARY_DIR} ${NMS_INCLUDE_DIR})

# The user visible library
add_library (nms libnms/fatal.cc libnms/option.cc)

if (HAVE_BFD)
  target_link_libraries (nms PRIVATE bfd)
endif ()
if (HAVE_DEMANGLE)
  target_link_libraries (nms PRIVATE iberty)
endif ()

# exports
set (NMS_INCLUDE_DIR ${NMS_INCLUDE_DIR} PARENT_SCOPE)
set (NMS_BINARY_DIR ${NMS_BINARY_DIR} PARENT_SCOPE)
