# Copyright (C) 2021-2023 Nathan Sidwell, nathan@acm.org
# License: Apache v2.0

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  message (NOTICE "Build tool:${CMAKE_GENERATOR}")
  if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug)
  endif ()
  message (NOTICE "Build type:${CMAKE_BUILD_TYPE}")

  # -g3 means we get macros
  set (CMAKE_CXX_FLAGS_DEBUG -g3)
  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_DEBUG}")
endif ()
