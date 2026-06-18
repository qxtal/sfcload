AC_INIT([sfcload], [1.0.0], [quartz@xtal.sh])
AC_CONFIG_SRCDIR([src/sfcload.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS

PKG_CHECK_MODULES([LIBSERIALPORT], [libserialport >= 0.1.1])
AC_CHECK_FUNCS([memmem]) dnl Check if we have memmem()

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
