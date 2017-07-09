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

#include <gtk/gtkversion.h>
// we only compile this file if GTK+ version is 2.10 or higher
#if GTK_CHECK_VERSION(2,10,0)

#include <unistd.h>

#include <gtk/gtkprintunixdialog.h>

#include "file_print_manager.h"
#include "gerror_handle.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

// uncomment the following if the specialist write_error() function in
// prog_defs.h is not available in the application in question
/*
#include <iostream>
inline void write_error(const char* message) {
  std::cerr << message;
}
*/

GobjHandle<GtkPrintSettings> FilePrintManager::print_settings_h;


namespace { // callback for internal use only

void FilePrintDialogCB::file_print_selected(GtkDialog*, int id, void* data) {
  FilePrintDialog* instance_p = static_cast<FilePrintDialog*>(data);
  if (id == GTK_RESPONSE_OK) instance_p->accepted();
  else instance_p->rejected();
  instance_p->close();
}

} // anonymous namespace

FilePrintDialog::FilePrintDialog(GtkWindow* parent_p, GtkPrintSettings* print_settings_p,
				 const char* caption, GdkPixbuf* window_icon_p):
                                    WinBase(caption, window_icon_p,
					    true, parent_p,
					    GTK_WINDOW(gtk_print_unix_dialog_new(0, 0))) {

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  g_signal_connect(G_OBJECT(get_win()), "response",
		   G_CALLBACK(FilePrintDialogCB::file_print_selected), this);

  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable(get_win(), false);

  if (print_settings_p) {
    gtk_print_unix_dialog_set_settings(GTK_PRINT_UNIX_DIALOG(get_win()), print_settings_p);
  }

  GtkPrintCapabilities capabilities = GtkPrintCapabilities(0);
  capabilities = GtkPrintCapabilities(GTK_PRINT_CAPABILITY_GENERATE_PS);

  gtk_print_unix_dialog_set_manual_capabilities(GTK_PRINT_UNIX_DIALOG(get_win()),
						capabilities);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

void FilePrintDialog::on_delete_event(void) {
  rejected();
  close();
}

GtkPrinter* FilePrintDialog::get_printer(void) const {
  return gtk_print_unix_dialog_get_selected_printer(GTK_PRINT_UNIX_DIALOG(get_win()));
}

GobjHandle<GtkPrintSettings> FilePrintDialog::get_settings(void) const {
  return GobjHandle<GtkPrintSettings>(gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(get_win())));
}

GtkPageSetup* FilePrintDialog::get_page_setup(void) const {
  return gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(get_win()));
}

namespace { // callback for internal use only

void FilePrintManagerCB::file_print_job_complete(GtkPrintJob*, void* data, GError* error) {
  FilePrintManager* instance_p = static_cast<FilePrintManager*>(data);
  if (error) {
    write_error(error->message);
    write_error("\n");
    // we shouldn't free the GError object - the print system will do
    // that (see gtk_print_backend_cups_print_stream(), which calls
    // cups_request_execute() with this callback as part of a user
    // data struct (ps argument) in a call to cups_print_cb() which
    // calls ps->callback and thus this callback function). I imagine
    // the lpr backend does something similar (the interface shouldn't
    // be different)
  }
  instance_p->clean_up();
  instance_p->unref();
}

}   // anonymous namespace

FilePrintManager::~FilePrintManager(void) {

  // empty the filename through a mutex to synchronise memory
  Thread::Mutex::Lock lock(mutex);
  filename = "";
}

IntrusivePtr<FilePrintManager> FilePrintManager::create_manager(GtkWindow* parent,
								const std::string& caption_text,
								GobjHandle<GdkPixbuf> icon_h) {

  FilePrintManager* instance_p = new FilePrintManager;
  instance_p->print_notifier.connect(sigc::mem_fun(*instance_p, &FilePrintManager::show_dialog));
  instance_p->parent_p = parent;
  instance_p->caption = caption_text;
  instance_p->window_icon_h = icon_h;

  Thread::Mutex::Lock lock(instance_p->mutex);
  instance_p->ready = true;

  return IntrusivePtr<FilePrintManager>(instance_p);
}

bool FilePrintManager::set_filename(const char* filename_, bool manage_file) {
  Thread::Mutex::Lock lock(mutex);
  if (!ready) return false;
  filename = filename_;
  manage = manage_file;
  return true;
}

bool FilePrintManager::print(void) {

  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    if (!ready) return false;
    // protect our innards
    ready = false;
  }

  // take ownership of ourselves
  ref();
  print_notifier();
  return true;
}

void FilePrintManager::show_dialog(void) {

  // hand back ownership to local scope so that if there is a problem
  // or an exception is thrown this method cleans itself up (we will
  // ref() again at the end of this method if all has gone well)
  IntrusivePtr<FilePrintManager> temp(this);

  mutex.lock();
  
  if (filename.empty()) {
    ready = true;
    mutex.unlock();
    write_error("No file has been specified for printing\n");
    return;
  }
  mutex.unlock();

  // this method is called via the Notifier object, so it must be executing
  // in the main GUI thread - it is therefore safe to call any GTK+ functions
  // in this method and in FilePrintManager::print_file()
  dialog_p = new FilePrintDialog(parent_p, print_settings_h.get(), caption.c_str(), window_icon_h);
  dialog_p->accepted.connect(sigc::mem_fun(*this, &FilePrintManager::print_file));
  dialog_p->rejected.connect(sigc::mem_fun(*this, &FilePrintManager::print_cancel));
  // there is no memory leak -- the memory will be deleted when the FilePrintDialog object closes
 
  // regain ownership of ourselves
  ref();
}

void FilePrintManager::print_file(void) {

  // hand back ownership to local scope so that if there is a problem
  // or an exception is thrown this method cleans itself up (we will
  // ref() again at the end of this method if all has gone well)
  IntrusivePtr<FilePrintManager> temp(this);

  GtkPrinter* printer_p = dialog_p->get_printer();
  if (!printer_p) {
    write_error(gettext("No valid printer selected\n"));
    clean_up();
  }
  else {
    print_settings_h = dialog_p->get_settings();
    GtkPageSetup* page_setup_p = dialog_p->get_page_setup();
    GobjHandle<GtkPrintJob> print_job_h(gtk_print_job_new("efax-gtk print job",
							  printer_p,
							  print_settings_h,
							  page_setup_p));
    GError* error_p = 0;
    bool result;
    { // scope block for mutex lock
      Thread::Mutex::Lock lock(mutex);
      result = gtk_print_job_set_source_file(print_job_h, filename.c_str(), &error_p);
    }
    if (!result) {
      if (error_p) {
	GerrorScopedHandle handle_h(error_p);
	write_error(error_p->message);
	write_error("\n");
      }
      clean_up();
    }
    else {
      // regain ownership of ourselves and print the job (the
      // print system will do the final unreference in the
      // FilePrintManagerCB::file_print_job_complete() function)
      ref();

      // gtk_print_job_send() will cause the print system to
      // acquire ownership of the GtkPrintJob object (so our
      // GobjHandle object can go out of scope)
      gtk_print_job_send(print_job_h,
			 FilePrintManagerCB::file_print_job_complete,
			 this, 0);
    }
  }
}

void FilePrintManager::print_cancel(void) {
  clean_up();
  unref();
}

void FilePrintManager::clean_up(void) {
  Thread::Mutex::Lock lock(mutex);
  if (manage && !filename.empty()) {
    unlink(filename.c_str());
  }
  ready = true;
}

#endif // GTK_CHECK_VERSION
