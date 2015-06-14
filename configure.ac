AC_PREREQ([2.69])
AC_INIT([ESip], [0.0.1], [gasmibal@gmail.com])
AC_CONFIG_HEADERS(src/inc/config.h)
LT_INIT
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_HEADER_STDC
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_MODULES([LIBEVENT], [libevent])
PKG_CHECK_MODULES([OSIP2], [libosip2])

# Checks for header files.
AC_CHECK_HEADERS([libcli.h], [])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIGNAL
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN
AC_HEADER_TIME
AC_TYPE_SIZE_T

# Checks for library functions.

AC_OUTPUT(Makefile src/Makefile tst/Makefile)
