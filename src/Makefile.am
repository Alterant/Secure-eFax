## Process this file with automake to produce Makefile.in

SUBDIRS = utils

bin_PROGRAMS = efax-gtk

efax_gtk_SOURCES = main.cpp mainwindow.cpp dialogs.cpp fax_list.cpp   \
	           fax_list_manager.cpp file_list.cpp                 \
                   efax_controller.cpp addressbook.cpp settings.cpp   \
                   settings_help.cpp helpfile.cpp socket_server.cpp   \
                   socket_list.cpp socket_notify.cpp logger.cpp       \
                   tray_icon.cpp libegg/eggtrayicon.c

noinst_HEADERS =   mainwindow.h dialogs.h fax_list.h                  \
	           fax_list_manager.h file_list.h efax_controller.h   \
                   addressbook.h settings.h settings_help.h           \
                   helpfile.h socket_server.h socket_list.h           \
                   socket_notify.h logger.h tray_icon.h menu_icons.h  \
		   addressbook_icons.h settings_icons.h window_icon.h \
                   socket_list_icons.h sigc_compatibility.h           \
                   file_list_icons.h fax_list_icons.h prog_defs.h     \
		   gpl.h fax_list_manager_icons.h libegg/eggtrayicon.h

INCLUDES = -DDATADIR=\"$(datadir)\" -DRC_DIR=\"$(sysconfdir)\" -DSIGC_VERSION=@SIGC_VER@

AM_CXXFLAGS = -D_XOPEN_SOURCE=600 @SIGC_CFLAGS@ @GTK_CFLAGS@ @GTHREAD_CFLAGS@ @GTK_UNIX_PRINT_CFLAGS@ -I./utils

AM_CFLAGS = @GTK_CFLAGS@ -I./libegg

efax_gtk_LDADD = utils/libutils.a @LIBINTL@ @SIGC_LIBS@ @GTK_LIBS@ @GTHREAD_LIBS@ @GTK_UNIX_PRINT_LIBS@

EXTRA_DIST = libegg/CREDITS
