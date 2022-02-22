dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)

AC_INIT(wmSMPmon, 4.0, varjat@gmail.com)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR(wmSMPmon)
AM_CONFIG_HEADER(wmSMPmon/config.h)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

dnl
dnl Specify paths to look for libraries and headers
dnl ===============================================
AC_ARG_WITH(libs-from,
       [  --with-libs-from        pass compiler flags to look for libraries],
       [lib_search_path="$withval $lib_search_path"])

AC_ARG_WITH(incs-from,
       [  --with-incs-from        pass compiler flags to look for header files],
       [inc_search_path="$withval $inc_search_path"])


dnl ===========================================
dnl             Stuff that uses X
dnl ===========================================

AC_PATH_XTRA

X_LIBRARY_PATH=$x_libraries

XCFLAGS="$X_CFLAGS"

XLFLAGS="$X_LIBS"

XLIBS="-lX11 $X_EXTRA_LIBS"

lib_search_path="$lib_search_path $XLFLAGS -L/usr/local/lib"
inc_search_path="$inc_search_path $XCFLAGS -I/usr/local/include"

AC_SUBST(X_LIBRARY_PATH)

dnl Shape extension
dnl ===============
AC_CHECK_LIB(Xext, XShapeCombineMask, [XLIBS="$XLIBS -lXext"],
    [echo "The shape extension stuff could not be found in the X client libraries"
     exit 1],
    $X_LIBS $X_EXTRA_LIBS -lX11)

dnl XPM library
dnl ===========
AC_CHECK_LIB(Xpm, XpmCreatePixmapFromData, [XLIBS="$XLIBS -lXpm"],
    [echo "The libXpm library was not found, but is necessary to build this library"
     exit 1],
    $X_LIBS $X_EXTRA_LIBS -lX11)

AC_SUBST(XCFLAGS)
AC_SUBST(XLFLAGS)
AC_SUBST(XLIBS)

AC_SUBST(X_EXTRA_LIBS)

dnl ===============================================
dnl             End of stuff that uses X
dnl ===============================================

dnl =========
dnl Debugging
dnl =========
AC_ARG_ENABLE(debug,
             [  --enable-debug          turn on debugging [default=on]],,enable_debug=no)
if test "$enable_debug" = yes; then
  DFLAGS="-Wall -g -ansi -pedantic"
  AC_DEFINE(DEBUG, 1, [use debug code])
fi
AC_SUBST(DFLAGS)


dnl ============
dnl Check for OS
dnl ============

case ${host_os} in
linux*)
  OS=linux
  ;;
solaris*)
  OS=solaris
  ;;
*)
  echo ""
  echo "Sorry, ${host_os} is not supported yet"
  echo ""
  exit 1
  ;;
esac

AC_SUBST(OS)
AM_CONDITIONAL([USE_LINUX], [test "$OS" = "linux"])
AM_CONDITIONAL([USE_SOLARIS], [test "$OS" = "solaris"])



dnl =============================
dnl Checks for library functions.
dnl =============================

LIBRARY_SEARCH_PATH="$lib_search_path"
HEADER_SEARCH_PATH="$inc_search_path"

AC_SUBST(LIBRARY_SEARCH_PATH)
AC_SUBST(HEADER_SEARCH_PATH)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h memory.h stddef.h stdlib.h string.h strings.h sys/param.h sys/time.h unistd.h signal.h errno.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS(select strtoul uname bzero select strcasecmp strstr)



dnl ============================
dnl Enable NVidia GPU card data
dnl ============================

AC_ARG_WITH([nvidia],
            [AS_HELP_STRING([--with-nvidia],
              [support GPU utilization and Video RAM usage reading from NVidia GPU @<:@default=check@:>@])],
            [],
            [with_nvidia=check])
          
          LIBNVML=
          AS_IF([test "x$with_nvidia" != xno],
            [AC_CHECK_LIB([nvidia-ml], [main],
              [AC_SUBST([LIBNVML], ["-lnvidia-ml"])
               AC_DEFINE([HAVE_NVIDIA], [1],
                         [Define if you have NVML])
              ],
              [if test "x$with_nvidia" != xcheck; then
                 AC_MSG_FAILURE(
                   [--with-nvidia was given, but test for NVML failed])
               fi
              ], )])

AC_CONFIG_FILES(Makefile \
		wmSMPmon/Makefile )
AC_OUTPUT
