#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([Jool], [3.4.3], [jool@nic.mx])
AC_CONFIG_SRCDIR([common/jool.c])
AM_INIT_AUTOMAKE([subdir-objects])

# Checks for programs.
AC_PROG_CC

# It's been added because automake requires it, since automake 1.14 it is no longer required.
AM_PROG_CC_C_O

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
AC_CHECK_FUNCS([inet_ntoa memset strcasecmp strtol])

# Checks for dependencies.
PKG_CHECK_MODULES(LIBNL3, libnl-3.0 >= 3.1)

# Spit out the makefiles.
AC_OUTPUT(Makefile stateless/Makefile stateful/Makefile)
