# NMS utils
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

# This happens to be my first CMake project, so it's undoubtably wonky

# Don't actually know the minimum version this'll work with, 3.18 is what I have
cmake_minimum_required (VERSION 3.18)

project (NMS VERSION 1.0 LANGUAGES CXX)

include (CheckLibraryExists)
include (CheckIncludeFileCXX)

set (CMAKE_CXX_STANDARD 20)

# HAVE_BINUTILS (rename to USE_BINUTILS)
option (NMS_CHECKING "Enable checking" ON)
option (NMS_BACKTRACE "Enable backtrace on crash" ON)

if (NMS_BACKTRACE)
  check_library_exists (bfd bfd_init "/usr/lib" HAVE_BFD)
  check_library_exists (iberty cplus_demangle "/usr/lib" HAVE_DEMANGLE)
  check_include_file_cxx ("ucontext.h" HAVE_UCONTEXT)
endif ()

# Intentionally containing project root
file (RELATIVE_PATH BIN2SRC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})

configure_file (config.h.in config.h ESCAPE_QUOTES)

configure_file (gdbinit.in gdbinit ESCAPE_QUOTES)

include_directories ("${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

# the user visible library
add_library (nms libnms/fatal.cc libnms/option.cc)

if (HAVE_BFD)
  target_link_libraries (nms PRIVATE bfd)
endif ()
if (HAVE_DEMANGLE)
  target_link_libraries (nms PRIVATE iberty)
endif ()
