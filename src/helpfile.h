/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef HELPFILE_H
#define HELPFILE_H

#include "prog_defs.h"

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtknotebook.h>

#include "utils/window.h"

namespace { // we put the functions in anonymous namespace in helpfile.cpp
            // so they are not exported at link time
namespace HelpDialogCB {
  extern "C" {
    void help_button_clicked(GtkWidget*, void*);
    gboolean help_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}
}   // anonymous namespace

class HelpDialog: public WinBase {
  static int is_helpfile;
  GtkNotebook* notebook_p;

  const char* get_sending_help(void);
  const char* get_receiving_help(void);
  const char* get_addressbook_help(void);
  const char* get_fax_list_help(void);
  const char* get_settings_help(void);
  GtkWidget* make_text_view(const char* text);
  GtkWidget* make_scrolled_window(void);
public:
  friend void HelpDialogCB::help_button_clicked(GtkWidget*, void*);
  friend gboolean HelpDialogCB::help_key_press_event(GtkWidget*, GdkEventKey*, void*);

  static int get_is_helpfile(void) {return is_helpfile;}
  HelpDialog(const int size);
  ~HelpDialog(void);
};

#endif
