/* Copyright (C) 2001 to 2007 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef FAX_LIST_H
#define FAX_LIST_H

#include "prog_defs.h"

// something in gtkprintjob.h clashes with the C++ headers with
// libstdc++-5 and it needs to be included here (via print_manager.h)
// to obtain a clean compile
#include <gtk/gtkversion.h>
#if GTK_CHECK_VERSION(2,10,0)
#include "utils/file_print_manager.h"
#include "utils/intrusive_ptr.h"
#include "utils/async_queue.h"
#endif

#include <string>
#include <utility>
#include <vector>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#include <sigc++/sigc++.h>

#include "fax_list_manager.h"
#include "utils/window.h"
#include "utils/pipes.h"
#include "utils/mutex.h"
#include "utils/shared_handle.h"

namespace { // we put the function in anonymous namespace in fax_list.cpp
            // so it is not exported at link time
namespace FaxListDialogCB {
  extern "C" void fax_list_button_clicked(GtkWidget*, void*);
}
}

class FaxListDialog: public sigc::trackable, public WinBase {
  FaxListEnum::Mode mode;
  static int is_fax_received_list;
  static int is_fax_sent_list;
  const int standard_size;

  Thread::Mutex working_dir_mutex;
  std::string working_dir;

  std::vector<SharedHandle<char*> > view_files;

  FaxListManager fax_list_manager;

#if GTK_CHECK_VERSION(2,10,0)
  AsyncQueue<IntrusivePtr<FilePrintManager> > async_queue;
#endif

  GtkWidget* close_button_p;
  GtkWidget* print_button_p;
  GtkWidget* view_button_p;
  GtkWidget* describe_button_p;
  GtkWidget* delete_fax_button_p;
  GtkWidget* empty_trash_button_p;
  GtkWidget* add_folder_button_p;
  GtkWidget* delete_folder_button_p;
  GtkWidget* reset_button_p;

  GtkWidget* new_fax_label_p;

  void set_buttons_slot(void);
  void describe_fax_prompt(void);
  void delete_fax(void);
  void add_folder_prompt(void);
  void add_folder(const std::string&);
  void empty_trash_prompt(void);
  void delete_folder_prompt(void);
  void write_pipe_to_file(PipeFifo*, int);
  std::pair<const char*, char* const*> get_print_from_stdin_parms(void);
  std::pair<const char*, char* const*> get_fax_to_ps_parms(const std::string&, bool);
  std::pair<const char*, char* const*> get_ps_viewer_parms(const char*);
  void print_fax_prompt(void);
  void print_fax(void);
  void print_fax_thread(std::string*, bool);
  void view_fax(void);
  void view_fax_thread(std::string*, std::pair<std::string*, int>);
  void delete_parms(std::pair<const char*, char* const*>);
public:
  friend void FaxListDialogCB::fax_list_button_clicked(GtkWidget*, void*);

  sigc::signal0<int> get_new_fax_count_sig;
  sigc::signal0<void> reset_sig;

  // display_new_fax_count() calls get_new_fax_count_sig, so until it has been
  // connected to a function which returns this, it will show 0 as the count
  void display_new_fax_count(void);
  static int get_is_fax_received_list(void) {return is_fax_received_list;}
  static int get_is_fax_sent_list(void) {return is_fax_sent_list;}
  void insert_new_fax_slot(const std::pair<std::string, std::string>& fax_info) {
    fax_list_manager.insert_new_fax_in_base(fax_info.first, fax_info.second);
    display_new_fax_count();
  }

  FaxListDialog(FaxListEnum::Mode, const int standard_size_);
  ~FaxListDialog(void);
};


namespace { // we put the functions in anonymous namespace in fax_list.cpp
            // so they are not exported at link time
namespace EntryDialogCB {
  extern "C" {
    void entry_dialog_selected(GtkWidget*, void*);
    gboolean entry_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}
}

class EntryDialog: public WinBase {

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
  GtkWidget* entry_p;

  bool selected_impl(void);
public:
  friend void EntryDialogCB::entry_dialog_selected(GtkWidget*, void*);
  friend gboolean EntryDialogCB::entry_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);

  sigc::signal1<void, const std::string&> accepted;
  EntryDialog(const int standard_size, const char* entry_text,
	      const char* caption, const char* label_text,
	      GtkWindow* parent_p);
};


class DescriptionDialog: public EntryDialog {

public:
  DescriptionDialog(const int standard_size, const char* text, GtkWindow* parent_p);
};

class AddFolderDialog: public EntryDialog {

public:
  AddFolderDialog(const int standard_size, GtkWindow* parent_p);
};

#endif
