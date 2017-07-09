/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef SOCKET_LIST_H
#define SOCKET_LIST_H

#include "prog_defs.h"

#include <string>
#include <list>
#include <utility>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>

#include <sigc++/sigc++.h>

#include "socket_server.h"
#include "utils/window.h"
#include "utils/gobj_handle.h"
#include "utils/shared_ptr.h"
#include "utils/mutex.h"


namespace { // we put the functions in anonymous namespace in socket_list.cpp
            // so they are not exported at link time
namespace SocketListDialogCB {
  extern "C" {
    void socket_list_button_clicked(GtkWidget*, void*);
    void socket_list_set_buttons(GtkTreeSelection*, void* data);
  }
}
}

class SocketListDialog: public sigc::trackable, public WinBase {
  static int is_socket_list;
  const int standard_size;

  GtkWidget* send_button_p;
  GtkWidget* view_button_p;
  GtkWidget* remove_button_p;
  GtkWidget* close_button_p;

  GobjHandle<GtkTreeModel> list_store_h;
  GtkTreeView* tree_view_p;

  void send_fax_impl(void);
  void view_file(void);
  void remove_file_prompt(void);
  void remove_file(void);
  std::pair<const char*, char* const*> get_view_file_parms(const std::string&);
  void delete_parms(std::pair<const char*, char* const*>);
protected:
  void on_delete_event(void);
public:
  friend void SocketListDialogCB::socket_list_button_clicked(GtkWidget*, void*);
  friend void SocketListDialogCB::socket_list_set_buttons(GtkTreeSelection*, void*);

  sigc::signal1<void, const std::pair<std::string, std::string>& > selected_fax;
  sigc::signal1<void, const std::string&> remove_from_socket_server_filelist;
  sigc::signal0<void> dialog_closed;

  void close_slot(void);

  void set_socket_list_rows(std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> >);
  static int get_is_socket_list(void) {return is_socket_list;}

  SocketListDialog(std::pair<SharedPtr<FilenamesList>,
		             SharedPtr<Thread::Mutex::Lock> > filenames_pair,
		   const int standard_size_);
  ~SocketListDialog(void);
};

#endif
