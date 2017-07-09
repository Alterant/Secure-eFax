/* Copyright (C) 2006 and 2007 Chris Vine

The library comprised in this file or of which this file is part is
distributed by Chris Vine under the GNU Lesser General Public
License as follows:

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License, version 2.1, for more details.

   You should have received a copy of the GNU Lesser General Public
   License, version 2.1, along with this library (see the file LGPL.TXT
   which came with this source code package in the src/utils sub-directory);
   if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA, 02111-1307, USA.

*/

#ifndef FILE_PRINTMANAGER_H
#define FILE_PRINTMANAGER_H

#include "prog_defs.h"

#include <gtk/gtkversion.h>
// we only compile this file if GTK+ version is 2.10 or higher
#if GTK_CHECK_VERSION(2,10,0)

#include <string>

#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkprinter.h>
#include <gtk/gtkprintsettings.h>
#include <gtk/gtkpagesetup.h>
#include <gtk/gtkprintjob.h>
#include <gdk/gdkpixbuf.h>

#include <sigc++/sigc++.h>

#include "window.h"
#include "notifier.h"
#include "mutex.h"
#include "gobj_handle.h"
#include "intrusive_ptr.h"

/* The FilePrintManager class prints a file printable by the print
   system (eg lpr or CUPS) using the GTK+ printer interface provided
   with GTK+ 2.10.0 onwards.  All print systems on Unix-like systems
   will print Postscript (PS) files.  Some may also print PDF files.
   To obtain a FilePrintManager object, call
   FilePrintManager::create_manager() in the thread in which the main
   GTK+ event loop runs.  FilePrintManager::set_filename(), which
   passes the name of the file to be printed, can be called in any
   thread.  To print the file, call FilePrintManager::print(), which
   may also be done in any thread.  If FilePrintManager::print()
   returned true, the FilePrintManager class will unlink() the file to
   be printed when it has been finished with unless false is passed as
   the second argument of FilePrintManager::set_filename(), even if
   printing failed.

   Once FilePrintManager::print() has been called, the
   FilePrintManager class owns a reference to itself and so manages
   its own lifetime - so once that method has been called it doesn't
   matter if the IntrusivePtr returned by
   FilePrintManager::create_manager() goes out of scope (however, once
   FilePrintManager::print() has been called, the object will not be
   deleted until both printing has completed or failed AND the
   IntrusivePtr returned by FilePrintManager::create_manager() has
   gone out of scope or has been reset()).

   If you want the print dialog which is displayed on calling
   FilePrintManager::print() to display a window icon, then pass
   FilePrintManager::create_manager() a GobjHandle<GdkPixbuf> icon as
   the second argument.

   Normally, you would probably only want to use any one
   FilePrintManager object to print a single print job (it has been
   designed with that in mind).  Nevertheless, if you keep a reference
   to the object alive via an active IntrusivePtr you can use it to
   print more than one print job.  However, if that is done it is not
   possible to call FilePrintManager::print() or
   FilePrintManager::set_filename() until the previous print job (if
   any) has been dispatched to the GTK+ print system (or cancelled).
   If the FilePrintManager object is not ready because it is in the
   middle of handling an earlier print job, then the call to
   FilePrintManager::print() or FilePrintManager::set_filename() will
   return false; if however the new print job was successfully started
   or file name successfully set, they will return true.  You do not
   need to check the return value of these methods for the first print
   job despatched by any one FilePrintManager object.

   For printing plain text (say, from a GtkTextBuffer class, or from a
   plain text file), see the TextPrintManager class in
   text_printmanager.h.
*/

namespace { // we put the function in anonymous namespace in file_print_manager.cpp
            // so it is not exported at link time
namespace FilePrintDialogCB {
  extern "C" void file_print_selected(GtkDialog*, int, void*);
}
}   // anonymous namespace

class FilePrintDialog: public WinBase {
protected:
  virtual void on_delete_event(void);
public:
  friend void FilePrintDialogCB::file_print_selected(GtkDialog*, int, void*);

  sigc::signal0<void> accepted;
  sigc::signal0<void> rejected;
  GtkPrinter* get_printer(void) const;
  GobjHandle<GtkPrintSettings> get_settings(void) const;
  GtkPageSetup* get_page_setup(void) const;
  FilePrintDialog(GtkWindow* parent_p, GtkPrintSettings* print_settings_p = 0,
		  const char* caption_p = 0, GdkPixbuf* window_icon_p = 0);
};

namespace { // we put the function in anonymous namespace in dialogs.cpp
            // so it is not exported at link time
namespace FilePrintManagerCB {
  extern "C" void file_print_job_complete(GtkPrintJob*, void*, GError*);
}
}   // anonymous namespace

class FilePrintManger;

class FilePrintManager: public sigc::trackable, public IntrusiveLockCounter {
  Thread::Mutex mutex;
  GtkWindow* parent_p;
  std::string caption;
  GobjHandle<GdkPixbuf> window_icon_h;
  bool manage;
  std::string filename;
  FilePrintDialog* dialog_p;
  Notifier print_notifier;
  bool ready;

  static GobjHandle<GtkPrintSettings> print_settings_h;

  void show_dialog(void);
  void print_file(void);
  void print_cancel(void);
  void clean_up(void);
  // private constructor
  FilePrintManager(void) {}
  // and this class may not be copied
  FilePrintManager(const FilePrintManager&);
  FilePrintManager& operator=(const FilePrintManager&);
public:
  friend void FilePrintManagerCB::file_print_job_complete(GtkPrintJob*, void*, GError*);
  static IntrusivePtr<FilePrintManager> create_manager(GtkWindow* parent = 0,
						       const std::string& caption = "",
						       GobjHandle<GdkPixbuf> window_icon_h = GobjHandle<GdkPixbuf>(0));
  bool set_filename(const char*, bool manage_file = true);
  bool print(void);
  ~FilePrintManager(void);
};

#endif // GTK_CHECK_VERSION
#endif // FILE_PRINTMANAGER_H
