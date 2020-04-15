# -*- mode:autoconf -*-
# Copyright (C) 2019-2020 Nathan Sidwell, nathan@acm.org
# Not For Distribution

AC_DEFUN([NMS_TOOLS],
[AC_MSG_CHECKING([tools])
AC_ARG_WITH([tools],
AS_HELP_STRING([--with-tools=DIR],[tool directory]),
if test "$withval" = "yes" ; then
  AC_MSG_ERROR([tool location not specified])
elif test "$withval" = "no" ; then
  :
elif ! test -d "$withval/bin" ; then
  AC_MSG_ERROR([tools not present])
else
  tools=$withval/bin
fi)
AC_MSG_RESULT($tools)
PATH=$tools${tools+:}$PATH
AC_SUBST(tools)])

AC_DEFUN([NMS_COMPILER],
[AC_ARG_WITH([compiler],
AS_HELP_STRING([--with-compiler=NAME],[which compiler to use]),
AC_MSG_CHECKING([compiler])
if test "$withval" = "yes" ; then
  AC_MSG_ERROR([NAME not specified])
elif test "$withval" = "no" ; then
  AC_MSG_ERROR([Gonna need a c++ compiler, dofus!])
else
  CXX="${withval}"
  AC_MSG_RESULT([$CXX])
fi)])

AC_DEFUN([NMS_CXX20],
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
#if  __cpp_concepts < 201907
#error "C++20 concepts required"
cpp_concepts is __cpp_concepts
#endif
#if __cpp_lib_int_pow2 < 202002
#error "std::has_single_bit required"
cpp_lib_int_pow2 is __cpp_lib_int_pow2
#endif
]])],
AC_MSG_RESULT([yes 🙂]),
AC_MSG_RESULT([no 🙁])
AC_MSG_ERROR([C++20 support is too immature]))])

AC_DEFUN([NMS_BUGURL],
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

AC_DEFUN([NMS_DISTRIBUTION],
[AC_ARG_ENABLE(distribution,
AS_HELP_STRING([--enable-distribution],
[enable distribution.  Inhibit components that prevent distribution]),,
[enable_distribution="no"])
case "$enable_distribution" in
  ("yes") nms_distribution=yes
         AC_MSG_ERROR([distribution is not permitted]) ;;
  ("no") nms_distribution=no ;;
  (*) AC_MSG_ERROR([unknown distribution $enable_distribution]) ;;
esac
AC_MSG_CHECKING([distribution])
AC_MSG_RESULT([$nms_distribution])])

AC_DEFUN([UTIL_CHECKING],
[AC_ARG_ENABLE(checking,
AS_HELP_STRING([--enable-checking],
[enable run-time checking]),,
[enable_checking="yes"])
case "$enable_checking" in
  ("yes") ac_checking=yes ;;
  ("no") ac_checking= ;;
  (*) AC_MSG_ERROR([unknown check "$check"]) ;;
esac
AC_MSG_CHECKING([checking])
AC_MSG_RESULT([${ac_checking:-no}])
if test "$ac_checking" = yes ; then
  AC_DEFINE(UTILS_CHECK, 1, [Enable checking])
fi])

AC_DEFUN([UTIL_BINUTILS],
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

AC_DEFUN([UTIL_BACKTRACE],
[AC_ARG_ENABLE(backtrace,
AS_HELP_STRING([--enable-backtrace],[provide backtrace on fatality.]),,
[enable_backtrace="maybe"])
if test "${util_backtrace:-maybe}" != no ; then
  AC_CHECK_HEADERS(execinfo.h)
  AC_CHECK_FUNCS(backtrace)
  if test $nms_distribution = no ; then
    AC_DEFINE(HAVE_DECL_BASENAME, 1, [Needed for demangle.h])
    # libiberty prevents distribution because of licensing
    AC_CHECK_HEADERS(demangle.h libiberty/demangle.h,break)
    # libbfd prevents distribution because of licensing
    AC_CHECK_HEADERS(bfd.h)
    AC_SEARCH_LIBS([bfd_openr],[bfd])
  fi
  if test "$ac_cv_func_backtrace" = yes ; then
    util_backtrace=yes
    ldbacktrace=-rdynamic
    AC_DEFINE(UTILS_BACKTRACE, 1, [Enable backtrace])
  elif test "$util_backtrace" = yes ; then
    AC_MSG_ERROR([Backtrace unavailable])
  fi
  AC_SUBST(ldbacktrace)
fi
AC_MSG_CHECKING([backtrace])
AC_MSG_RESULT([${util_backtrace:-no}])])

AC_DEFUN([IBLIS_NATIVE],
[dnl we only support native builds, because we run the engine to extend itself,
dnl literally writing out an augmented executable from the memory image
if test "$cross_compiling" = yes ; then
  AC_MSG_ERROR([Cross compiling not supported])
fi])

AC_DEFUN([IBLIS_64],
[AC_CHECK_SIZEOF(void *)
if test "$ac_cv_sizeof_void_p" != 8 ; then
  AC_MSG_ERROR([64 bit architecture is required])
fi])

AC_DEFUN([IBLIS_MEMORY],
[AC_MSG_CHECKING([memory range])
case $host in
(x86_64-*-linux*)
	# 4GB->64TB,32GB
	iblis_lwm=0x100000000;iblis_hwm=0x400000000000;iblis_size=0x800000000
	iblis_reloc=R_X86_64_64
	;;
(aarch64-*-linux*)
	# 4GB->256GB,32GB
	iblis_lwm=0x100000000;iblis_hwm=0x4000000000;iblis_size=0x800000000
	iblis_reloc=R_AARCH64_ABS64
	;;
(powerpc64-*-linux* | powerpc64le-*-linux*)
	# 2TB->32TB,32GB
	iblis_lwm=0x2000000000;iblis_hwm=200000000000;iblis_size=0x800000000
	iblis_reloc=R_PPC64_ADDR64
	;;
(*)
	AC_MSG_ERROR([host $host not supported])	;;
esac
AC_MSG_RESULT([[$iblis_lwm->$iblis_hwm,$iblis_size $iblis_reloc]])
AC_DEFINE_UNQUOTED(IBLIS_LWM, [uintptr_t ($iblis_lwm)], [IBLIS LWM])
AC_DEFINE_UNQUOTED(IBLIS_HWM, [uintptr_t ($iblis_hwm)], [IBLIS HWM])
AC_DEFINE_UNQUOTED(IBLIS_SIZE, [uintptr_t ($iblis_size)], [IBLIS SIZE])
AC_DEFINE_UNQUOTED(IBLIS_RELOC, [($iblis_reloc)], [IBLIS RELOC])
AC_SUBST(iblis_lwm)
AC_SUBST(iblis_size)

[iblis_fixed=".iblis.fixed"]
[iblis_object=".iblis.aged.object"]
[iblis_cons=".iblis.aged.cons"]
AC_DEFINE_UNQUOTED(IBLIS_FIXED, ["$iblis_fixed"], [fixed root section])
AC_SUBST(iblis_fixed)
AC_DEFINE_UNQUOTED(IBLIS_OBJECT, ["$iblis_object"], [aged object section])
AC_SUBST(iblis_object)
AC_DEFINE_UNQUOTED(IBLIS_CONS, ["$iblis_cons"], [aged cons section])
AC_SUBST(iblis_cons)])
