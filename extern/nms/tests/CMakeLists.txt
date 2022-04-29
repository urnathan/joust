# NMS utils
# Copyright (C) 2022 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

add_custom_target (check-nms
  srcdir=${CMAKE_CURRENT_SOURCE_DIR} PATH=${JOUST_BINARY_DIR}:$ENV{PATH}
  JOUST=nms.defs aloy
  -t kratos -o nms -g ${ZSH} -g ${CMAKE_CURRENT_SOURCE_DIR}/jouster
  COMMENT "Jousting"
  USES_TERMINAL)
add_dependencies (check check-nms)

file (GLOB_RECURSE FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CMakeLists.txt)
foreach (FILE ${FILES})
  get_filename_component (DIR ${FILE} PATH)
  if (DIR)
    add_subdirectory (${DIR})
  endif ()
endforeach ()

file (GENERATE OUTPUT nms.defs CONTENT
  "testdir=${CMAKE_CURRENT_SOURCE_DIR}
NMS_JOUSTING=$<BOOL:${NMS_JOUSTING}>
NMS_CHECKING=$<BOOL:${NMS_CHECKING}>
NMS_BACKTRACE=$<BOOL:${NMS_BACKTRACE}>
timelimit=60
memlimit=0
cpulimit=60
filelimit=1
SHELL=${ZSH}")
