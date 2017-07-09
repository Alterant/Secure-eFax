dnl Process this file with autoconf to produce a configure script.
AC_INIT([efax-gtk], [3.0.17])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME,AC_PACKAGE_VERSION)

AM_CONFIG_HEADER(config.h)

dnl check for working c and c++ compilers
AC_CHECK_COMPILERS
dnl Checks for programs.
AC_PROG_INSTALL

dnl Checks for libraries.
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.0.0)
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.0.0)
PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.0.0)
PKG_CHECK_GTK_UNIX_PRINT
PKG_CHECK_SIGC
AC_CHECK_HEADERS([pthread.h],[],[AC_MSG_ERROR([pthread.h not found], 1)])
AC_CHECK_X11_XLIB_H
dnl this is probably best not checked for
dnl AC_CHECK_GTHREAD_HAS_PTHREADS

dnl Checks for typedefs, structures, and compiler characteristics.
AC_LANG_CPLUSPLUS dnl switch to C++
AC_TYPE_SIZE_T
AC_CHECK_TYPES(ssize_t)
AC_CHECK_SOCKLEN_T
AC_CHECK_IN_ADDR_T

dnl set up gettext
AM_GNU_GETTEXT([external])

dnl Checks for library functions.
AC_CHECK_MKSTEMP
AC_CHECK_HAVE_IOS_NOCREATE
AC_CHECK_HAVE_ATTACH
AC_CHECK_HAVE_STRINGSTREAM
AC_CHECK_HAVE_STREAM_IMBUE
AC_CHECK_HEADERS([ostream istream])

dnl now check out the install directory
AC_INSTALL_DIRS

dnl enable source in subdirectories to be compiled
AC_PROG_RANLIB

dnl define MAKE for subdirectories
AC_PROG_MAKE_SET

AC_CONFIG_FILES([ Makefile src/Makefile efax/Makefile po/Makefile.in \
		  efax-gtk-faxfilter/Makefile src/utils/Makefile efax-gtk.spec ])
AC_OUTPUT

AC_CLOSING_MESSAGE
