# Joust: Journal Of User-Scripted Tests	-*- mode:cmake -*-
# Copyright (C) 2020-2024 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

file (GENERATE OUTPUT joust.defs CONTENT
"testdir=${CMAKE_CURRENT_SOURCE_DIR}
timelimit=60
memlimit=1
cpulimit=60
filelimit=1
SHELL=${ZSH}")

add_custom_target (check-joust
  srcdir=${CMAKE_CURRENT_SOURCE_DIR} PATH=${Joust_BINARY_DIR}:$ENV{PATH}
  JOUST=joust.defs aloy
  -t kratos -o joust -g ${CMAKE_CURRENT_SOURCE_DIR}/jouster
  DEPENDS joust
  COMMENT "Jousting ${PROJECT_NAME}"
  VERBATIM USES_TERMINAL)
add_dependencies (check check-joust)

function (add_test_executables)
  file (GLOB TESTSRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS *.cc)
  foreach (TESTSRC ${TESTSRCS})
    get_filename_component (TESTPROG ${TESTSRC} NAME_WLE)
    add_executable (${TESTPROG} EXCLUDE_FROM_ALL ${TESTSRC})
    target_link_libraries (${TESTPROG} libjoust libnms)
    nms_ident_dependency (${TESTPROG})
    add_dependencies (check-joust ${TESTPROG})
  endforeach ()
endfunction ()

add_subdirectory (02-kratos)
add_subdirectory (04-joust)

# add_dependencies (check check-joust)
