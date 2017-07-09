/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef DIALOGS_H
#define DIALOGS_H

#include "prog_defs.h"

#include <string>
#include <vector>
#include <utility>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkstyle.h>

#include <gdk/gdkevents.h>

#include <sigc++/sigc++.h>

#include "utils/window.h"


namespace { // we put the functions in anonymous namespace in dialogs.cpp
            // so they are not exported at link time
namespace FileReadSelectDialogCB {
  extern "C" {
    void file_selected(GtkWidget*, void*);
    void file_view_file(GtkWidget*, void*);
  }
}
}   // anonymous namespace

class FileReadSelectDialog: public WinBase {
  int standard_size;

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;

  std::vector<std::string> result;
  std::pair<const char*, char* const*> get_view_file_parms(void);
  void delete_parms(std::pair<const char*, char* const*>);
  std::string get_filename_string(void);
  void set_result(void);
  void view_file_impl(void);
protected:
  virtual void on_delete_event(void);
public:
  friend void FileReadSelectDialogCB::file_selected(GtkWidget*, void*);
  friend void FileReadSelectDialogCB::file_view_file(GtkWidget*, void*);

  // get_result() returns the filenames in UTF-8 (not filesystem) encoding
  std::vector<std::string> get_result(void) const {return result;}
  FileReadSelectDialog(int standard_size, bool multi_files, GtkWindow* parent_p);
};

namespace { // we put the functions in anonymous namespace in dialogs.cpp
            // so they are not exported at link time
namespace GplDialogCB {
  extern "C" {
    void gpl_selected(GtkWidget*, void*);
    gboolean gpl_key_press_event(GtkWidget*, GdkEventKey*, void*);
    void gpl_style_set(GtkWidget*, GtkStyle*, void*);
  }
}
}   // anonymous namespace

class GplDialog: public WinBase {
public:
  enum Result {rejected, accepted};
private:
  int standard_size;
  Result result;
  GtkWidget* accept_button_p;
  GtkWidget* reject_button_p;
  GtkTextView* text_view_p;
  std::string max_text;
  void get_longest_button_text(void);
protected:
  virtual int get_exec_val(void) const;
  virtual void on_delete_event(void);
public:
  friend void GplDialogCB::gpl_selected(GtkWidget*, void*);
  friend gboolean GplDialogCB::gpl_key_press_event(GtkWidget*, GdkEventKey*, void*);
  friend void GplDialogCB::gpl_style_set(GtkWidget*, GtkStyle*, void*);

  GplDialog(int standard_size);
};

namespace { // we put the functions in anonymous namespace in dialogs.cpp
            // so they are not exported at link time
namespace InfoDialogCB {
  extern "C" void info_selected(GtkDialog*, int, void*);
}
}   // anonymous namespace

class InfoDialog: public WinBase {
public:
  friend void InfoDialogCB::info_selected(GtkDialog*, int, void*);

  InfoDialog(const char* text, const char* caption,
	     GtkMessageType message_type, GtkWindow* parent_p, bool modal = true);
};


namespace { // we put the functions in anonymous namespace in dialogs.cpp
            // so they are not exported at link time
namespace PromptDialogCB {
  extern "C" void prompt_selected(GtkWidget*, void*);
}
}   // anonymous namespace

class PromptDialog: public WinBase {
  bool result;
  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
protected:
  virtual int get_exec_val(void) const;
  virtual void on_delete_event(void);
public:
  friend void PromptDialogCB::prompt_selected(GtkWidget*, void*);

  sigc::signal0<void> accepted;
  sigc::signal0<void> rejected;

  PromptDialog(const char* text, const char* caption,
	       int standard_size, GtkWindow* parent_p, bool modal = true);
};

#endif
