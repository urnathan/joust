# Joust: Journal Of User-Scripted Tests	-*- mode:autoconf -*-
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

AC_INIT([joust],[0.0],[github.com/urnathan/joust])
AC_CONFIG_SRCDIR(README.md)
m4_include(config.m4)

AC_CONFIG_AUX_DIR(build-aux)
AC_SUBST(PACKAGE_VERSION)

NMS_NOT_IN_SOURCE
AC_CANONICAL_HOST

NMS_TOOLS
NMS_NUM_CPUS
NMS_MAINTAINER_MODE
NMS_CXX_COMPILER
AC_LANG(C++)
AC_PROG_CXX
NMS_CXX_20
NMS_TOOL_DIRS
NMS_LINK_OPT([-Wl,--no-undefined])
NMS_CONFIG_FILES([gdbinit])

NMS_BUGURL
NMS_DISTRIBUTION
NMS_ENABLE_CHECKING
NMS_WITH_BINUTILS
NMS_ENABLE_BACKTRACE

AC_CONFIG_HEADER(config.h)
AC_CHECK_TOOL([AR],[ar])
AH_VERBATIM([_GNU_SOURCE],[#define _GNU_SOURCE 1])
AH_VERBATIM([_FORTIFY_SOURCE],[#undef _FORTIFY_SOURCE])

AC_CHECK_FUNCS([mremap])
AC_CHECK_FUNCS([pipe2])
AC_CHECK_FUNCS([epoll_create1 signalfd])
AH_TEMPLATE([USE_EPOLL],[Use epoll/signalfd rather than pselect])
if test "$ac_cv_func_epoll_create1$ac_cv_func_signalfd" = yesyes ; then
  AC_DEFINE([USE_EPOLL])
else
  AC_CHECK_FUNC([pselect],,
    [AC_MSG_ERROR([neither epoll/signalfd nor pselect found])])
fi
AC_CHECK_TYPES([ucontext_t],[],[],[#include <ucontext.h>])

AC_OUTPUT
