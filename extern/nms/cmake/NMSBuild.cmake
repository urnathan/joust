# Copyright (C) 2021 Nathan Sidwell, nathan@acm.org
# License: Apache v2.0

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  message (NOTICE "Build tool:${CMAKE_GENERATOR}")
  if (CMAKE_GENERATOR STREQUAL Ninja)
    file (CONFIGURE OUTPUT Makefile CONTENT
"# Generated Forwarding Makefile: DO NOT EDIT
# why? because fingers type 'make'
# Copyright (C) 2019-2020 Nathan Sidwell, nathan@acm.org
# License: Apache v2.0

ifneq (,$(MAKECMDGOALS))
$(MAKECMDGOALS): ninja
endif

NINJA_OPTIONS :=

ifneq (,$(VERBOSE))
NINJA_OPTIONS += -v
endif

ninja:
	@echo Forwarding to: ninja $(NINJA_OPTIONS) $(MAKECMDGOALS)
	@ninja $(NINJA_OPTIONS) $(MAKECMDGOALS)
" @ONLY NEWLINE_STYLE UNIX)
  endif ()

  if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug)
  endif ()
  message (NOTICE "Build type:${CMAKE_BUILD_TYPE}")

  set (CMAKE_CXX_FLAGS_DEBUG -g3)
  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_DEBUG}")
endif ()
