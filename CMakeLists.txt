# Joust: Journal Of User-Scripted Tests	-*- mode:autoconf -*-
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

# This happens to be my first CMake project, so it's undoubtably wonky

# Don't actually know the minimum version this'll work with, 3.18 is what I have
cmake_minimum_required (VERSION 3.18)

project (Joust VERSION 1.0 LANGUAGES CXX)

include (CheckLibraryExists)
include (CheckSymbolExists)
include (CheckIncludeFileCXX)

set (CMAKE_BUILD_TYPE Debug)
set (CMAKE_CXX_STANDARD 20)

# HAVE_BINUTILS (rename to USE_BINUTILS)
option (NMS_CHECKING "Enable checking" ON)
option (NMS_BACKTRACE "Enable backtrace on crash" ON)

if (NMS_BACKTRACE)
  check_library_exists (bfd bfd_init "/usr/lib" HAVE_BFD)
  check_library_exists (iberty cplus_demangle "/usr/lib" HAVE_DEMANGLE)
  check_include_file_cxx ("ucontext.h" HAVE_UCONTEXT)
endif ()

check_symbol_exists (pipe2 "unistd.h" HAVE_PIPE2)
check_symbol_exists (mremap "sys/mman.h" HAVE_MREMAP)

# epoll & signalfd || pselect?
check_symbol_exists (epoll_create1 "sys/epoll.h" HAVE_EPOLL)
check_symbol_exists (signalfd "sys/signalfd.h" HAVE_SIGNALFD)
if (HAVE_EPOLL AND HAVE_SIGNALFD)
  set (USE_EPOLL YES)
else ()
  check_symbol_exists (pselect "sys/select.h" HAVE_PSELECT)
  if (NOT HAVE_PSELECT)
    message (FATAL_ERROR "Neither epoll_create1(2) nor pselect(2) available")
  endif ()
endif ()

set (PROJECT_NAME "Joust")
set (PROJECT_VERSION "${CMAKE_PROJECT_VERSION}")
set (PROJECT_URL "https://github.com/urnathan/joust")
file (RELATIVE_PATH BIN2SRC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})

configure_file (config.in.h config.h ESCAPE_QUOTES)

configure_file (gdbinit.in .gdbinit ESCAPE_QUOTES)

include_directories ("${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/include")

add_compile_options ("-W;-Wall;-Woverloaded-virtual")
add_compile_options ("-fdiagnostics-color=always;-fno-diagnostics-show-option")
add_compile_options ("-fno-exceptions;-fno-rtti")

# the user visible library
add_library (joust src/tester.cc)
install (TARGETS joust DESTINATION lib)
install (FILES include/joust/tester.hh DESTINATION include/joust)

# our internal library
add_library (internal STATIC
  src/error.cc src/fatal.cc src/lexer.cc src/option.cc
  src/readBuffer.cc src/regex.cc src/scanner.cc src/spawn.cc src/symbols.cc
  src/token.cc)
set_source_files_properties (src/regex.cc PROPERTIES COMPILE_OPTIONS
  "-fexceptions;-frtti")
if (HAVE_BFD)
  target_link_libraries (internal PRIVATE bfd)
endif ()
if (HAVE_DEMANGLE)
  target_link_libraries (internal PRIVATE iberty)
endif ()

# our executables
set (PROGS aloy ezio kratos)
foreach (PROG ${PROGS})
  add_executable (${PROG} src/${PROG}.cc)
  target_link_libraries (${PROG} PRIVATE joust internal)
  install (TARGETS ${PROG} DESTINATION bin)
endforeach ()

# tests
add_subdirectory (tests)
