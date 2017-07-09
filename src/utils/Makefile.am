## Process this file with automake to produce Makefile.in

noinst_LIBRARIES =    libutils.a

libutils_a_SOURCES =  io_watch.cpp notifier.cpp              \
		      pipes.cpp  selected_rows_handle.cpp    \
		      thread.cpp  utf8_utils.cpp             \
		      widget.cpp window.cpp                  \
		      toolbar_append_widget.cpp              \
		      file_print_manager.cpp                 \
		      text_print_manager.cpp

noinst_HEADERS =      fdstream.h fdstream.tcc                \
		      gobj_handle.h icon_info_handle.h       \
		      io_watch.h mutex.h notifier.h          \
		      pipes.h selected_rows_handle.h         \
		      sem_sync.h shared_handle.h             \
		      shared_ptr.h intrusive_ptr.h thread.h  \
		      tree_path_handle.h utf8_utils.h        \
	              tree_row_reference_handle.h widget.h   \
		      window.h toolbar_append_widget.h       \
		      async_queue.h gerror_handle.h          \
		      file_print_manager.h		     \
		      text_print_manager.h callback.h        \
		      pango_layout_iter_handle.h

INCLUDES = -DDATADIR=\"$(datadir)\" -DRC_DIR=\"$(sysconfdir)\" -DSIGC_VERSION=@SIGC_VER@

AM_CXXFLAGS = -D_XOPEN_SOURCE=600 @SIGC_CFLAGS@ @GTK_CFLAGS@ @GTHREAD_CFLAGS@ @GTK_UNIX_PRINT_CFLAGS@ -I..

EXTRA_DIST = LGPL.TXT
