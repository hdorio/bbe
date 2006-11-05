dnl Process this file with autoconf to produce a configure script.

AC_INIT(bbe, 0.2.2, tjsa@iki.fi)
AC_CONFIG_SRCDIR(src/bbe.c)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AC_CONFIG_HEADER(config.h)

AC_SUBST(VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

AC_CANONICAL_HOST

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC
AC_GNU_SOURCE

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(features.h error.h errno.h getopt.h)
AC_CHECK_FUNCS(getline getopt_long) 

    
dnl Checks for typedefs, structures, and compiler characteristics.
AC_SYS_LARGEFILE
AC_SYS_LONG_FILE_NAMES
AC_CHECK_TYPES(off_t)
AC_CHECK_TYPES(long long)
AC_C_INLINE

dnl Checks for library functions.

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile])
AC_OUTPUT
