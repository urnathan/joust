# Joust: Journal Of User-Scripted Tests	-*- mode:cmake -*-
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

# This happens to be my first CMake project, so it's undoubtably wonky

# Don't actually know the minimum version this'll work with, 3.18 is what I have
cmake_minimum_required (VERSION 3.18)

list (PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list (PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/extern/nms/cmake)
include (NMSBuild)

if (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  message (FATAL_ERROR "Not intended as a subcomponent")
endif ()
project (Joust VERSION 1.0 LANGUAGES CXX)
set (PROJECT_URL "https://github.com/urnathan/joust")

include (CheckLibraryExists)
include (CheckSymbolExists)
include (CheckIncludeFileCXX)

set (CMAKE_CXX_STANDARD 20)

add_compile_options ("-fdiagnostics-color=always;-fno-diagnostics-show-option")
add_compile_options ("-W;-Wall;-Woverloaded-virtual")
add_compile_options ("-fno-exceptions;-fno-rtti")

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

add_subdirectory (extern/nms EXCLUDE_FROM_ALL)
configure_file (gdbinit.in .gdbinit ESCAPE_QUOTES)

configure_file (joustcfg.h.in joustcfg.h ESCAPE_QUOTES)
include_directories ("${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${NMS_INCLUDE_DIR}")

# the user visible library
file (GLOB JOUST_SRCS CONFIGURE_DEPENDS "libjoust/*.cc")
file (GLOB JOUST_HDRS CONFIGURE_DEPENDS "include/joust/*.hh")
add_library (joust ${JOUST_SRCS})
install (TARGETS joust DESTINATION lib)
install (FILES ${JOUST_HDRS} DESTINATION include/joust)

# gaige Group All Internal Gizmo Elements
file (GLOB GAIGE_SRCS CONFIGURE_DEPENDS "libgaige/*.cc")
add_library (gaige STATIC ${GAIGE_SRCS})
set_source_files_properties (libgaige/regex.cc PROPERTIES COMPILE_OPTIONS
  "-fexceptions;-frtti")
target_link_libraries (gaige PRIVATE nms)

# our executables
set (PROGS aloy ezio kratos)
foreach (PROG ${PROGS})
  add_executable (${PROG} progs/${PROG}.cc)
  target_link_libraries (${PROG} PRIVATE joust gaige nms)
  install (TARGETS ${PROG} DESTINATION bin)
endforeach ()

add_custom_target (check)

# tests
add_subdirectory (tests)
