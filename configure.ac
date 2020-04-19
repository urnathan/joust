# Joust: Journal Of User-Scripted Tests	-*- mode:autoconf -*-
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

AC_INIT([joust],[0.0],[nathan@acm.org])
AC_CONFIG_SRCDIR(README.md)
m4_include(nms/config.m4)

AC_CONFIG_AUX_DIR(nms/build-aux)
AC_SUBST(PACKAGE_VERSION)

AC_CANONICAL_HOST

NMS_TOOLS
AC_LANG(C++)
AC_PROG_CXX
NMS_CXX_COMPILER
NMS_CXX_20

NMS_BUGURL
NMS_DISTRIBUTION

NMS_PROJECT_ENABLE

CONFIG_FILES+=" gdbinit"
NMS_CONFIG_FILES

AC_OUTPUT
