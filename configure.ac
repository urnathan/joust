# Joust: Journal Of User-Scripted Tests	-*- mode:autoconf -*-
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

AC_INIT([joust],[0.0],[github.com/urnathan/joust])
AC_CONFIG_SRCDIR(README.md)
m4_include(config.m4)

AC_CONFIG_AUX_DIR(build-aux)
AC_SUBST(PACKAGE_VERSION)

AC_CANONICAL_HOST

JOUST_TOOLS
AC_LANG(C++)
AC_PROG_CXX
JOUST_CXX_COMPILER
JOUST_CXX_20

JOUST_BUGURL
JOUST_DISTRIBUTION

JOUST_PROJECT_ENABLE

CONFIG_FILES+=" gdbinit"
JOUST_CONFIG_FILES

AC_OUTPUT
