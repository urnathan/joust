# Joust: Journal Of User-Scripted Tests	-*- mode:cmake -*-
# Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

cmake_minimum_required (VERSION 3.21)

project (Joust VERSION 1.0
  HOMEPAGE_URL "https://github.com/urnathan/joust"
  DESCRIPTION "A testsuite framework"
  LANGUAGES CXX)

if (PROJECT_IS_TOP_LEVEL)
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/nms/CMakeLists.txt")
    message (FATAL_ERROR
      "git submodules not downloaded, try git submodule update")
  endif ()
  set (CMAKE_CXX_STANDARD 20)
  list (PREPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/extern/nms/cmake)
  include (NMSUtils)
  nms_build_defaults ()
  add_custom_target (check)
  add_subdirectory (extern/nms EXCLUDE_FROM_ALL)
endif ()

add_custom_target (joust)
nms_ident (NMS)

include (CheckLibraryExists)
include (CheckSymbolExists)
include (CheckIncludeFileCXX)

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

configure_file (gdbinit.in .gdbinit ESCAPE_QUOTES)
configure_file (cfg.h.in include/joust/cfg.h @ONLY)

# user visible library
file (GLOB JOUST_SRCS CONFIGURE_DEPENDS "joust/*.cc")
add_library (libjoust ${JOUST_SRCS})
add_dependencies (joust libjoust)
set_property(TARGET libjoust PROPERTY OUTPUT_NAME joust)
target_include_directories (libjoust
  PUBLIC include ${Joust_BINARY_DIR}/include)
target_link_libraries (libjoust PUBLIC libnms)

# Internal library, gaige Group All Internal Gizmo Elements
file (GLOB GAIGE_SRCS CONFIGURE_DEPENDS "gaige/*.cc")
add_library (libgaige STATIC ${GAIGE_SRCS})
add_dependencies (joust libgaige)
set_property(TARGET libgaige PROPERTY OUTPUT_NAME gaige)
target_include_directories (libgaige
  PUBLIC include ${Joust_BINARY_DIR}/include)
set_source_files_properties (gaige/regex.cc PROPERTIES COMPILE_OPTIONS
  "-fexceptions;-frtti")
target_link_libraries (libgaige PRIVATE libnms)

# our executables
set (PROGS aloy ezio kratos)
foreach (PROG ${PROGS})
  add_executable (${PROG} progs/${PROG}.cc)
  target_link_libraries (${PROG} PRIVATE libjoust libgaige libnms)
  add_dependencies (joust ${PROG})
endforeach ()

nms_ident_dependency (${PROGS})

# tests
add_subdirectory (tests)
