

file (GENERATE OUTPUT joust.defs CONTENT
"testdir=${CMAKE_CURRENT_SOURCE_DIR}
timelimit=60
memlimit=1
cpulimit=60
filelimit=1
SHELL=zsh")

add_custom_target (check
  srcdir=${CMAKE_SOURCE_DIR} PATH="${CMAKE_BINARY_DIR}:$ENV{PATH}"
  JOUST=joust.defs aloy
  -t kratos -o joust -g ${CMAKE_CURRENT_SOURCE_DIR}/jouster
  COMMENT "Jousting"
  USES_TERMINAL)
add_dependencies (check ${PROGS})

function (add_test_executables)
  include_directories ("${CMAKE_SOURCE_DIR}/src")
  file (GLOB TESTSRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS *.cc)
  foreach (TESTSRC ${TESTSRCS})
    string (REGEX REPLACE "\\.cc$" "" TESTPROG ${TESTSRC})
    add_executable (${TESTPROG} EXCLUDE_FROM_ALL ${TESTSRC})
    target_link_libraries (${TESTPROG} joust nms)
    add_dependencies (check ${TESTPROG})
  endforeach ()
endfunction ()

add_subdirectory (02-kratos)
add_subdirectory (04-joust)
