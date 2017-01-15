m4_define([version_major], [0])
m4_define([version_minor], [0])
m4_define([version_micro], [0])

AC_INIT([compgc], [version_major.version_minor.version_micro])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([./])

AC_CONFIG_AUX_DIR([build/autoconf])
AC_CONFIG_MACRO_DIR([build/autoconf])
AM_INIT_AUTOMAKE([subdir-objects])

AM_PROG_AR

LT_INIT

AX_VALGRIND_CHECK()

AC_ARG_ENABLE(debug,
[AS_HELP_STRING([--enable-debug], [enable debugging])],
[], [enable_debug=no])

CFLAGS=                         dnl get rid of default -g -O2
COMMON_CFLAGS="-Wall -Wformat -Wformat-security -Wextra -Wunused \
-Wshadow -Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wcast-align \
-Wstrict-prototypes -Wredundant-decls -Wendif-labels -Wcast-qual \
-std=gnu11 -Wpedantic"

if test "x$enable_debug" = x"yes"; then
  EXTRA_CFLAGS="-O0 -g"
else
  EXTRA_CFLAGS="-O3"
  AC_DEFINE(NDEBUG,1,[Define whether debugging is enabled])
fi
AC_SUBST(COMMON_CFLAGS)
AC_SUBST(EXTRA_CFLAGS)

dnl XXX: i don't know why this doesn't work
dnl AC_SEARCH_LIBS(garble_new, garble, [], AC_MSG_ERROR([libgarble not found]))
AC_SEARCH_LIBS(SHA1_Init, [crypto], [], AC_MSG_ERROR([libcrypto not found]))
AC_SEARCH_LIBS(__gmpz_init, [gmp], [], AC_MSG_ERROR([libgmp not found]))
AC_SEARCH_LIBS(json_loads, [jansson], [], AC_MSG_ERROR([libjansson not found]))
AC_SEARCH_LIBS(floor, [m], [], AC_MSG_ERROR([libm not found]))
AC_SEARCH_LIBS(msgpack_unpack_next, [msgpack, msgpackc], [], AC_MSG_ERROR([libmsgpack not found]))

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
