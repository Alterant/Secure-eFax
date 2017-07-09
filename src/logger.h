/* Copyright (C) 2001 to 2007 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef LOGGER_H
#define LOGGER_H

#include "prog_defs.h"

#include <string>
#include <fstream>
#include <exception>

#include <gtk/gtkwindow.h>
#include <gtk/gtkwidget.h>
#include <gdk/gdkevents.h>
#include <glib/gtypes.h>

#include "utils/window.h"


namespace { // we put the function in anonymous namespace in logger.cpp
            // so it is not exported at link time
namespace LoggerCB {
  extern "C" gboolean logger_logfile_timer(void*);
}
}

class Logger {

  int logfile_count;
  std::ofstream logfile;
  std::string logfile_name;
  std::string default_dirname;
  guint timer_tag;
  GtkWindow* parent_p;

  void logfile_timer_impl(void);
  bool log_to_string(std::string&);
public:
  friend gboolean LoggerCB::logger_logfile_timer(void*);

  void write_to_log(const char*);
  void reset_logfile(void);

  // only call print_log() and view_log() in the main GUI thread.
  // print_log() and view_log() will compile but won't do anything
  // unless this program is compiled against GTK+-2.10.0 or higher,
  // so that the GTK+ printing API is available
  void print_log(void);
  static void print_page_setup(GtkWindow* parent_p);
  void view_log(const int standard_size);
  
  Logger(const char* default_dir, GtkWindow* parent_p = 0);
  ~Logger(void);
};

namespace { // we put the functions in anonymous namespace in helpfile.cpp
            // so they are not exported at link time
namespace LogViewDialogCB {
  extern "C" {
    void log_view_button_clicked(GtkWidget*, void*);
    gboolean log_view_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}
}   // anonymous namespace

struct LogViewFileError: public std::exception {
  virtual const char* what() const throw() {return "Cannot open log file for viewing";}
};

class LogViewDialog: public WinBase {
  GtkWidget* text_view_p;
public:
  friend void LogViewDialogCB::log_view_button_clicked(GtkWidget*, void*);
  friend gboolean LogViewDialogCB::log_view_key_press_event(GtkWidget*, GdkEventKey*, void*);
  LogViewDialog(const int standard_size, const std::string& logfile_name, GtkWindow* parent_p = 0);
};


#endif
