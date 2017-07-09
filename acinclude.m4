dnl acinclude.m4 -- various bits of this have been borrowed from the m4
dnl configuration files for KDE and GNOME

AC_DEFUN([AC_INSTALL_DIRS],
[
AC_MSG_CHECKING(the fax spool directory)
dnl set up the defaults

SPOOLDIR="/var/spool/fax"
AC_ARG_WITH(spooldir,
  [  --with-spooldir=DIR     Where fax printer filter is installed (default is /var/spool/fax)],
[
if test "$withval" != "no"; dnl 
  then SPOOLDIR="$withval"
fi
])
AC_MSG_RESULT([SPOOLDIR is $SPOOLDIR])

AC_SUBST(SPOOLDIR)
])

dnl By checking for the libraries to be linked against this
dnl only checks for dynamic linking and therefore may not
dnl be very useful
AC_DEFUN([AC_CHECK_GTHREAD_HAS_PTHREADS],
[
  AC_CACHE_VAL(ac_cv_gthread_has_pthreads, [

  succeeded=no

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then
        AC_MSG_CHECKING(whether glib compiled against pthreads)

	GT_LIBS=`$PKG_CONFIG --libs "gthread-2.0"`

        T_STRING=${GT_LIBS%pthread*}
	if test ${#GT_LIBS} -gt ${#T_STRING}; then
	  succeeded=yes
        fi
     else
        echo "*** Your version of pkg-config is too old. You need version $PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig"
     fi
  fi

  if test $succeeded = yes; then
    ac_cv_gthread_has_pthreads="yes"
    AC_MSG_RESULT(yes)
  else
    ac_cv_gthread_has_pthreads="no"
    AC_MSG_ERROR([glib does not appear to be compiled against pthreads])
  fi
  ])
])

AC_DEFUN([AC_CHECK_HAVE_IOS_NOCREATE],
[
  AC_MSG_CHECKING(for fstream ios::nocreate flag)
  AC_CACHE_VAL(ac_cv_have_ios_nocreate,
  [
    AC_LANG_CPLUSPLUS
    AC_COMPILE_IFELSE(
    [
      AC_LANG_PROGRAM(
      [
#include <fstream>
      ],
      [
        std::fstream strm("dummy.txt", std::ios::in | std::ios::nocreate);
      ])
    ],
    [ac_cv_have_ios_nocreate="yes"],
    [ac_cv_have_ios_nocreate="no"]
    ) dnl end AC_COMPILE_IFELSE
  ])
  AC_MSG_RESULT([$ac_cv_have_ios_nocreate])
  if test "$ac_cv_have_ios_nocreate" = "yes" ; then
     AC_DEFINE(HAVE_IOS_NOCREATE, 1, [ Define if the C++ fstream object has ios::nocreate ])
  fi 
])

AC_DEFUN([AC_CHECK_HAVE_ATTACH],
[
  AC_MSG_CHECKING(for fstream attach)
  AC_CACHE_VAL(ac_cv_have_attach,
  [
    AC_LANG_CPLUSPLUS
    AC_COMPILE_IFELSE(
    [
      AC_LANG_PROGRAM(
      [
#include <fstream>
      ],
      [
        std::fstream strm;
        strm.attach(1);
      ])
    ],
    [ac_cv_have_attach="yes"],
    [ac_cv_have_attach="no"]
    ) dnl end AC_COMPILE_IFELSE
  ])
  AC_MSG_RESULT([$ac_cv_have_attach])
  if test "$ac_cv_have_attach" = "yes" ; then
     AC_DEFINE(HAVE_ATTACH, 1, [ Define if the C++ fstream object has fstream::attach(int fd) funtion ])
  fi 
])

AC_DEFUN([AC_CHECK_HAVE_STRINGSTREAM],
[
  AC_MSG_CHECKING(for std::stringstream)
  AC_CACHE_VAL(ac_cv_have_stringstream,
  [
    AC_LANG_CPLUSPLUS
    AC_COMPILE_IFELSE(
    [
      AC_LANG_PROGRAM([
#include <sstream>
      ],
      [
        std::ostringstream strm;
      ])
    ],
    [ac_cv_have_stringstream="yes"],
    [ac_cv_have_stringstream="no"]
    ) dnl end AC_COMPILE_IFELSE
  ])
  AC_MSG_RESULT([$ac_cv_have_stringstream])
  if test "$ac_cv_have_stringstream" = "yes" ; then
     AC_DEFINE(HAVE_STRINGSTREAM, 1, [ Define if the C++ library contains std::stringstream ])
  fi 
])

AC_DEFUN([AC_CHECK_HAVE_STREAM_IMBUE],
[
  AC_MSG_CHECKING(whether streams may have a locale imbued)
  AC_CACHE_VAL(ac_cv_have_stream_imbue,
  [
    AC_LANG_CPLUSPLUS
    AC_COMPILE_IFELSE(
    [
      AC_LANG_PROGRAM([
#include <iostream>
#include <locale>
      ],
      [
        std::cout.imbue(std::locale::classic());
      ])
    ],
    [ac_cv_have_stream_imbue="yes"],
    [ac_cv_have_stream_imbue="no"]
    ) dnl end AC_COMPILE_IFELSE
  ])
  AC_MSG_RESULT([$ac_cv_have_stream_imbue])
  if test "$ac_cv_have_stream_imbue" = "yes" ; then
     AC_DEFINE(HAVE_STREAM_IMBUE, 1, [ Define if the C++ library can imbue stream objects with a locale ])
  fi 
])

AC_DEFUN([AC_CHECK_SOCKLEN_T],
[
  AC_CHECK_TYPES(socklen_t, , ,
  [
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
  ])
])

AC_DEFUN([AC_CHECK_IN_ADDR_T],
[
  AC_CHECK_TYPES(in_addr_t, , ,
  [
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
  ])
])

AC_DEFUN([AC_CHECK_COMPILERS],
[
  AC_ARG_ENABLE(debug,[  --enable-debug 	  creates debugging code [default=no]],
  [ 
   if test $enableval = "no"; dnl 
     then ac_use_debug_code="no"
     else ac_use_debug_code="yes"
   fi
  ], [ac_use_debug_code="no"])

  dnl Just for configure --help

  dnl this prevents stupid AC_PROG_CC from adding "-g" to the default CFLAGS
  c_flags_in=$CFLAGS

  AC_PROG_CC
  AC_PROG_CPP

  CFLAGS=$c_flags_in

  if test -z "$CFLAGS"; then 
    if test "$GCC" = "yes"; then
      if test "$ac_use_debug_code" = "yes"; then
        CFLAGS="-g $CFLAGS"
      else
        CFLAGS="-O2 $CFLAGS"
      fi
    else 
      if test "$ac_use_debug_code" = "yes"; then
        AC_CHECK_C_COMPILER_FLAG(g,
          [
            CFLAGS="-g $CFLAGS"
        ])
      else 
        AC_CHECK_C_COMPILER_FLAG(O2,
          [
            CFLAGS="-O2 $CFLAGS"
        ])
      fi
    fi
    AC_CHECK_C_COMPILER_FLAG(pthread,
      [
        CFLAGS="$CFLAGS -pthread"
    ])
    if test "$GCC" = "yes"; then
       CFLAGS="$CFLAGS -Wall"
    fi
  fi

  dnl this prevents stupid AC_PROG_CXX from adding "-g" to the default CXXFLAGS
  cxx_flags_in=$CXXFLAGS

  AC_PROG_CXX
  AC_PROG_CXXCPP

  CXXFLAGS=$cxx_flags_in

  if test -z "$CXXFLAGS"; then 
    if test "$GXX" = "yes"; then
      if test "$ac_use_debug_code" = "yes"; then
        CXXFLAGS="-g $CXXFLAGS"
      else
        CXXFLAGS="-O2 $CXXFLAGS"
        if test -z "$LDFLAGS"; then 
          LDFLAGS="-s"
        fi
      fi
    else 
      if test "$ac_use_debug_code" = "yes"; then
        AC_CHECK_COMPILER_FLAG(g,
          [
            CXXFLAGS="-g $CXXFLAGS"
        ])
      else 
        AC_CHECK_COMPILER_FLAG(O2,
          [
            CXXFLAGS="-O2 $CXXFLAGS"
        ])
      fi
    fi

    AC_CHECK_COMPILER_FLAG(fexceptions,
      [
        CXXFLAGS="$CXXFLAGS -fexceptions"
    ])

    AC_CHECK_COMPILER_FLAG(frtti,
      [
        CXXFLAGS="$CXXFLAGS -frtti"
    ])

    AC_CHECK_COMPILER_FLAG(fsigned-char,
      [
        CXXFLAGS="$CXXFLAGS -fsigned-char"
    ])

    AC_CHECK_COMPILER_FLAG(fno-check-new,
      [
        CXXFLAGS="$CXXFLAGS -fno-check-new"
    ])

    AC_CHECK_COMPILER_FLAG(pthread,
      [
        CXXFLAGS="$CXXFLAGS -pthread"
    ])

    if test "$GXX" = "yes"; then
       CXXFLAGS="$CXXFLAGS -Wall"
    fi
  fi  
])

AC_DEFUN([AC_CHECK_COMPILER_FLAG],
[
AC_MSG_CHECKING(whether $CXX supports -$1)
flag_cache=`echo $1 | sed 'y%.=/+-%___p_%'`
AC_CACHE_VAL(ac_cv_prog_cxx_$flag_cache,
[
echo 'int main() { return 0; }' >conftest.cc
eval "ac_cv_prog_cxx_$flag_cache=no"
if test -z "`$CXX -$1 -c conftest.cc 2>&1`"; then
  if test -z "`$CXX -$1 -o conftest conftest.o 2>&1`"; then
    eval "ac_cv_prog_cxx_$flag_cache=yes"
  fi
fi
rm -f conftest*
])
if eval "test \"`echo '$ac_cv_prog_cxx_'$flag_cache`\" = yes"; then
 AC_MSG_RESULT(yes)
 :
 $2
else
 AC_MSG_RESULT(no)
 :
 $3
fi
])

AC_DEFUN([AC_CHECK_C_COMPILER_FLAG],
[
AC_MSG_CHECKING(whether $CC supports -$1)
flag_cache=`echo $1 | sed 'y%.=/+-%___p_%'`
AC_CACHE_VAL(ac_cv_prog_c_$flag_cache,
[
echo 'int main() { return 0; }' >conftest.cc
eval "ac_cv_prog_c_$flag_cache=no"
if test -z "`$CC -$1 -c conftest.cc 2>&1`"; then
  if test -z "`$CC -$1 -o conftest conftest.o 2>&1`"; then
    eval "ac_cv_prog_c_$flag_cache=yes"
  fi
fi
rm -f conftest*
])
if eval "test \"`echo '$ac_cv_prog_c_'$flag_cache`\" = yes"; then
 AC_MSG_RESULT(yes)
 :
 $2
else
 AC_MSG_RESULT(no)
 :
 $3
fi
])

AC_DEFUN([AC_CHECK_MKSTEMP],
[
  AC_LANG_C
  ac_cflags_safe=$CFLAGS
  ac_ldflags_safe=$LDFLAGS
  ac_libs_safe=$LIBS
  CFLAGS=""
  LDFLAGS=""
  LIBS=""
  AC_CHECK_FUNCS(mkstemp,,
    AC_MSG_ERROR([Library function mkstemp is required and cannot be found]))
  CFLAGS=$ac_cflags_safe
  LDFLAGS=$ac_ldflags_safe
  LIBS=$ac_libs_safe
])

AC_DEFUN([AC_CHECK_X11_XLIB_H],
[

  ac_cflags_safe=$CFLAGS

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then

        if $PKG_CONFIG --exists gdk-2.0; then
          CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gdk-2.0`"
        fi
     else
        echo "*** Your version of pkg-config is too old. You need version $PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig"
     fi
  fi
  AC_CHECK_HEADERS([X11/Xlib.h])

  CFLAGS=$ac_cflags_safe
])

AC_DEFUN([AC_CLOSING_MESSAGE],
[
echo
echo "    Configuration complete."
echo
echo "    To compile, enter \`make', and then enter \`make install'"
echo "      -- \`make install' must be run as root."
echo
echo "    To reconfigure, enter \`make clean' and then run \`./configure' again."
echo
])

dnl PKG_CHECK_MODULES(GSTUFF, gtk+-2.0 >= 1.3 glib = 1.3.4, action-if, action-not)
dnl defines GSTUFF_LIBS, GSTUFF_CFLAGS, see pkg-config man page
dnl also defines GSTUFF_PKG_ERRORS on error
AC_DEFUN([PKG_CHECK_MODULES], [
  succeeded=no

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then
        AC_MSG_CHECKING(for $2)

        if $PKG_CONFIG --exists "$2" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING($1_CFLAGS)
            $1_CFLAGS=`$PKG_CONFIG --cflags "$2"`
            AC_MSG_RESULT($$1_CFLAGS)

            AC_MSG_CHECKING($1_LIBS)
            $1_LIBS=`$PKG_CONFIG --libs "$2"`
            AC_MSG_RESULT($$1_LIBS)
        else
            $1_CFLAGS=""
            $1_LIBS=""
            AC_MSG_RESULT([no])
        fi

        AC_SUBST($1_CFLAGS)
        AC_SUBST($1_LIBS)
     else
        echo "*** Your version of pkg-config is too old. You need version $PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig"
     fi
  fi

  if test $succeeded = yes; then
     ifelse([$3], , :, [$3])
  else
     ifelse([$4], , AC_MSG_ERROR([Library requirements ($2) not met; consider adjusting the PKG_CONFIG_PATH environment variable if your libraries are in a nonstandard prefix so pkg-config can find them.]), [$4])
  fi
])

dnl PKG_CHECK_VERSION does the same as PKG_CHECK_MODULES except
dnl that it does not set *_CFLAGS and *_LIBS variables
AC_DEFUN([PKG_CHECK_VERSION], [
  succeeded=no

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then
        AC_MSG_CHECKING(for $2)

        if $PKG_CONFIG --exists "$2" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes
        else
            AC_MSG_RESULT([no])
        fi
     else
        echo "*** Your version of pkg-config is too old. You need version $PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig"
     fi
  fi

  if test $succeeded = yes; then
     ifelse([$3], , :, [$3])
  else
     ifelse([$4], , AC_MSG_ERROR([Library requirements ($2) not met; consider adjusting the PKG_CONFIG_PATH environment variable if your libraries are in a nonstandard prefix so pkg-config can find them.]), [$4])
  fi
])

AC_DEFUN([PKG_CHECK_SIGC], [

  SIGC_VER=0
  PKG_CHECK_MODULES(SIGC, sigc++-2.0 >= 2.0.0,
  [
    SIGC_VER=20
  ],
  [
    PKG_CHECK_MODULES(SIGC, sigc++-1.2 >= 1.2.3)
    SIGC_VER=12
  ])
  AC_SUBST(SIGC_VER)
])

AC_DEFUN([PKG_CHECK_GTK_UNIX_PRINT], [

  PKG_CHECK_MODULES(GTK_UNIX_PRINT, gtk+-unix-print-2.0 >= 2.10.0, ,
  [
    GTK_UNIX_PRINT_CFLAGS=""
    GTK_UNIX_PRINT_LIBS=""
    AC_SUBST(GTK_UNIX_PRINT_CFLAGS)
    AC_SUBST(GTK_UNIX_PRINT_LIBS)
  ])
])
