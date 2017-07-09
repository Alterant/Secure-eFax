/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef FILE_LIST_H
#define FILE_LIST_H

#include "prog_defs.h"

#include <string>
#include <utility>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>

#include <sigc++/sigc++.h>

#include "utils/window.h"
#include "utils/gobj_handle.h"


namespace { // we put the functions in anonymous namespace in file_list.cpp
            // so they are not exported at link time
namespace FileListDialogCB {
  extern "C" {
    void file_list_button_clicked(GtkWidget*, void*);
    void file_list_set_buttons(GtkTreeSelection*, void* data);
  }
}
}

class FileListDialog: public sigc::trackable, public WinBase {
  static int is_file_list;
  const int standard_size;

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
  GtkWidget* up_button_p;
  GtkWidget* down_button_p;
  GtkWidget* add_button_p;
  GtkWidget* view_button_p;
  GtkWidget* remove_button_p;

  GobjHandle<GtkTreeModel> list_store_h;
  GtkTreeView* tree_view_p;

  std::string get_files(void);
  void add_file(void);
  void view_file(void);
  void remove_file_prompt(void);
  void remove_file(void);
  void move_up(void);
  void move_down(void);
  std::pair<const char*, char* const*> get_view_file_parms(const std::string&);
  void delete_parms(std::pair<const char*, char* const*>);
public:
  friend void FileListDialogCB::file_list_button_clicked(GtkWidget*, void*);
  friend void FileListDialogCB::file_list_set_buttons(GtkTreeSelection*, void*);

  sigc::signal1<void, const std::string&> accepted;

  static int get_is_file_list(void) {return is_file_list;}

  FileListDialog(const int standard_size, GtkWindow* parent_p);
  ~FileListDialog(void);
};

#endif
