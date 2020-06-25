# Joust: Journal Of User-Scripted Tests	-*- mode:autoconf -*-
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

AC_INIT([joust],[0.0],[github.com/urnathan/joust])
AC_CONFIG_SRCDIR(README.md)
m4_include(config.m4)

AC_CONFIG_AUX_DIR(build-aux)
AC_SUBST(PACKAGE_VERSION)

AC_CANONICAL_HOST

JOUST_TOOL_BIN
JOUST_CXX_COMPILER
AC_LANG(C++)
AC_PROG_CXX
JOUST_CXX_20
JOUST_LINK_OPT([-Wl,--no-undefined])
JOUST_TOOL_LIB
JOUST_TOOL_INC

JOUST_BUGURL
JOUST_DISTRIBUTION
JOUST_ENABLE_CHECKING
JOUST_WITH_BINUTILS
JOUST_ENABLE_BACKTRACE

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

for generated in config.h.in configure ; do
  if test $srcdir/configure.ac -nt $srcdir/$generated ; then
    touch $srcdir/$generated
  fi
done

CONFIG_FILES="Makefile gdbinit"
for make in Makesub tests/Makesub ; do
  test -f ${srcdir}/$make.in && CONFIG_FILES+=" $make"
done
AC_CONFIG_FILES([$CONFIG_FILES])

AC_SUBST(configure_args,[$ac_configure_args])
AC_SUBST(CONFIG_FILES)

AC_OUTPUT
