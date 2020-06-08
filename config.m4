# Joust: Journal Of User-Scripted Tests -*- mode:autoconf -*-
# Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
# License: Affero GPL v3.0

AC_DEFUN([JOUST_TOOLS],
[AC_MSG_CHECKING([tools])
AC_ARG_WITH([tools],
AS_HELP_STRING([--with-tools=DIR],[tool directory]),
if test "$withval" = "yes" ; then
  AC_MSG_ERROR([tool location not specified])
elif test "$withval" = "no" ; then
  :
elif ! test -d "${withval%/bin}/bin" ; then
  AC_MSG_ERROR([tools not present])
else
  tools=${withval%/bin}/bin
fi)
AC_MSG_RESULT($tools)
PATH=$tools${tools+:}$PATH
AC_SUBST(tools)])

AC_DEFUN([JOUST_CXX_COMPILER],
[AC_ARG_WITH([compiler],
AS_HELP_STRING([--with-compiler=NAME],[which compiler to use]),
AC_MSG_CHECKING([C++ compiler])
if test "$withval" = "yes" ; then
  AC_MSG_ERROR([NAME not specified])
elif test "$withval" = "no" ; then
  AC_MSG_ERROR([Gonna need a c++ compiler, dofus!])
else
  CXX="${withval}"
  AC_MSG_RESULT([$CXX])
fi)])

AC_DEFUN([JOUST_CXX_20],
[AC_MSG_CHECKING([whether $CXX supports C++20])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#if __cplusplus <= 201703
#error "C++20 is required"
#endif
]])],
[AC_MSG_RESULT([yes])],
[CXX+=" -std=c++20"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#if __cplusplus <= 201703
#error "C++20 is required"
#endif
]])],
AC_MSG_RESULT([adding -std=c++20]),
AC_MSG_RESULT([no])
AC_MSG_ERROR([C++20 is required])]))

AC_MSG_CHECKING([whether C++20 support is sufficiently advanced])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <version>
// There doesn't seem to be a feature macro for __VA_OPT__ :(
#define VARIADIC(X,...) X __VA_OPT__((__VA_ARGS__))
#define X(Y,Z) 1
int ary[VARIADIC(X,Y,Z)];
#if  __cpp_constinit < 201907
#error "C++20 constinit required"
cpp_constinit is __cpp_constinit
#endif
#if  __cpp_if_constexpr < 201606
#error "C++20 constexpr required"
cpp_constexpr is __cpp_if_constexpr
#endif
#if  __cpp_concepts < 201907
#error "C++20 concepts required"
cpp_concepts is __cpp_concepts
#endif
#if __cpp_structured_bindings < 201606
#error "C++20 structured bindings required"
cpp_structured_bindings is __cpp_structured_bindings
#endif
#if __cpp_lib_int_pow2 < 202002
#error "std::has_single_bit required"
cpp_lib_int_pow2 is __cpp_lib_int_pow2
#endif
]])],
AC_MSG_RESULT([yes 🙂]),
AC_MSG_RESULT([no 🙁])
AC_MSG_ERROR([C++20 support is too immature]))])

AC_DEFUN([JOUST_BUGURL],
[AC_MSG_CHECKING([bugurl])
AC_ARG_WITH(bugurl,
AS_HELP_STRING([--with-bugurl=URL],[where to report bugs]),
if test "$withval" = "yes" ; then
  AC_MSG_ERROR([URL not specified])
elif test "$withval" = "no" ; then
  BUGURL=""
else
  BUGURL="${withval}"
fi,BUGURL="${PACKAGE_BUGREPORT}")
AC_MSG_RESULT($BUGURL)
AC_DEFINE_UNQUOTED(BUGURL,"$BUGURL",[Bug reporting location])])

AC_DEFUN([JOUST_DISTRIBUTION],
[AC_ARG_ENABLE(distribution,
AS_HELP_STRING([--enable-distribution],
[enable distribution.  Inhibit components that prevent distribution]),,
[enable_distribution="no"])
case "$enable_distribution" in
  ("yes") joust_distribution=yes ;;
  ("no") joust_distribution=no ;;
  (*) AC_MSG_ERROR([unknown distribution $enable_distribution]) ;;
esac
AC_MSG_CHECKING([distribution])
AC_MSG_RESULT([$joust_distribution])])

AC_DEFUN([JOUST_ENABLE_CHECKING],
[AC_ARG_ENABLE(checking,
AS_HELP_STRING([--enable-checking],
[enable run-time checking]),,
[enable_checking="yes"])
case "$enable_checking" in
  ("yes") joust_checking=yes ;;
  ("no") joust_checking= ;;
  (*) AC_MSG_ERROR([unknown check "$enable_check"]) ;;
esac
AC_MSG_CHECKING([checking])
AC_MSG_RESULT([${joust_checking:-no}])
if test "$joust_checking" = yes ; then
  AC_DEFINE([JOUST_CHECKING], [1], [Enable checking])
fi])

AC_DEFUN([JOUST_WITH_BINUTILS],
[AC_MSG_CHECKING([binutils])
AC_ARG_WITH(bfd,
AS_HELP_STRING([--with-bfd=DIR], [location of libbfd]),
if test "$withval" = "yes" ; then
  AC_MSG_ERROR([DIR not specified])
elif test "$withval" = "no" ; then
  AC_MSG_RESULT(installed)
else
  AC_MSG_RESULT(${withval})
  CPPFLAGS="${CPPFLAGS} -I${withval}/include"
  LDFLAGS="${LDFLAGS} -L${withval}/lib"
fi,
AC_MSG_RESULT(installed))])

AC_DEFUN([JOUST_ENABLE_BACKTRACE],
[AC_REQUIRE([JOUST_DISTRIBUTION])
AC_ARG_ENABLE(backtrace,
AS_HELP_STRING([--enable-backtrace],[provide backtrace on fatality.]),,
[enable_backtrace="maybe"])
if test "${enable_backtrace:-maybe}" != no ; then
  AC_CHECK_HEADERS(execinfo.h)
  AC_CHECK_FUNCS(backtrace)
  if test "$joust_distribution" = no ; then
    AC_DEFINE([HAVE_DECL_BASENAME], [1], [Needed for demangle.h])
    # libiberty prevents distribution because of licensing
    AC_CHECK_HEADERS([demangle.h libiberty/demangle.h],[break])
    # libbfd prevents distribution because of licensing
    AC_CHECK_HEADERS([bfd.h])
    AC_SEARCH_LIBS([bfd_openr],[bfd],[LIBS+="-lz -liberty -ldl"],,[-lz -liberty -ldl])
  fi
  if test "$ac_cv_func_backtrace" = yes ; then
    joust_backtrace=yes
    ldbacktrace=-rdynamic
    AC_DEFINE([JOUST_BACKTRACE], [1], [Enable backtrace])
  elif test "$enable_backtrace" = yes ; then
    AC_MSG_ERROR([Backtrace unavailable])
  fi
  AC_SUBST([ldbacktrace])
fi
AC_MSG_CHECKING([backtrace])
AC_MSG_RESULT([${joust_backtrace:-no}])])
