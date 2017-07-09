/* Copyright (C) 2001 to 2007 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "prog_defs.h"

#include <unistd.h>
#include <sys/types.h>

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <memory>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtktexttag.h>
// there is a problem with the GTK+ headers in some versions: before including gtktextmark.h
// we need to include gtktextiter.h or there will be a compile error in main.cpp
#include <gtk/gtktextiter.h>
#include <gtk/gtktextmark.h>

#include <gdk/gdkevents.h>
#include <gdk/gdkgc.h>
#include <glib/gtypes.h>

#include <sigc++/sigc++.h>


#include "efax_controller.h"
#include "fax_list_manager.h"
#include "fax_list.h"
#include "socket_server.h"
#include "socket_list.h"
#include "tray_icon.h"
#include "helpfile.h"
#include "logger.h"
#include "utils/pipes.h"
#include "utils/shared_ptr.h"
#include "utils/mutex.h"
#include "utils/widget.h"
#include "utils/gobj_handle.h"
#include "utils/notifier.h"
#include "utils/io_watch.h"
#include "utils/utf8_utils.h"

#define MAINWIN_SAVE_FILE ".efax-gtk_mainwin_save"


class MessageText: public sigc::trackable, public MainWidgetBase {

  GtkWidget* text_view_p;
  GtkTextMark* end_mark_p;
  GobjHandle<GtkTextTag> red_tag_h;
  std::auto_ptr<Logger> logger_a;
  GtkWindow* window_p;

  void cleanify(std::string&);
public:
  void write_black_slot(const char* message);
  void write_red_slot(const char* message);
  void print_log(void) {logger_a->print_log();}
  static void print_page_setup(GtkWindow* parent_p) {Logger::print_page_setup(parent_p);}
  void view_log(const int standard_size) {logger_a->view_log(standard_size);}
  void reset_logfile(void) {logger_a->reset_logfile();}
  MessageText(GtkWindow* window_p);
};

class StatusLine: public sigc::trackable, public MainWidgetBase {

  const int standard_size;
  GtkWidget* status_label_p;
public:
  void set_status_line(int);
  void write_status_slot(const char* text);
  StatusLine(const int);
};

namespace { // we put the functions in anonymous namespace in mainwindow.cpp
            // so they are not exported at link time
namespace MainWindowCB {
  extern "C" {
    void mainwin_button_clicked(GtkWidget*, void*);
    void mainwin_menuitem_activated(GtkMenuItem*, void*);
    gboolean mainwin_key_press_event(GtkWidget*, GdkEventKey*, void*);
    gboolean mainwin_visibility_notify_event(GtkWidget*, GdkEventVisibility*, void*);
    gboolean mainwin_window_state_event(GtkWidget*, GdkEventWindowState*, void*);
    void mainwin_style_set(GtkWidget*, GtkStyle*, void*);
    gboolean mainwin_timer_event(void*);
    gboolean mainwin_drawing_area_expose_event(GtkWidget*, GdkEventExpose*, void*);
    gboolean mainwin_start_hidden_check(void*);
  }
}
}

class MainWindow: public sigc::trackable, public WinBase {

  const int standard_size;
  static PipeFifo error_pipe;
  static bool connected_to_stderr;
  bool obscured;
  bool minimised;
  pid_t prog_pid;
  std::string prog_fifo_name;
  guint error_pipe_tag;

  sigc::connection socket_dialog_connection;
  sigc::connection update_socket_list_connection;
  sigc::signal1<void, std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> > > update_socket_list;
  sigc::signal0<void> close_socket_list_dialog;
  Notifier present_window_notify;

  GtkWidget* drawing_area_p;
  GtkWidget* file_button_p;
  GtkWidget* socket_button_p;
  GtkWidget* file_entry_p;
  GtkWidget* number_entry_p;
  //GtkWidget* pass_entry_p;
  GtkWidget* single_file_button_p;
  GtkWidget* multiple_file_button_p;
  GtkWidget* socket_list_button_p;
  GtkWidget* number_button_p;
  GtkWidget* send_button_p;
  GtkWidget* receive_answer_button_p;
  GtkWidget* receive_takeover_button_p;
  GtkWidget* receive_standby_button_p;
  GtkWidget* stop_button_p;

  GtkMenuItem* list_received_faxes_item_p;
  GtkMenuItem* list_sent_faxes_item_p;
  GtkMenuItem* socket_list_item_p;
  GtkMenuItem* single_file_item_p;
 // GtkMenuItem* multiple_file_item_p;
  GtkMenuItem* address_book_item_p;
  GtkMenuItem* settings_item_p;
  GtkMenuItem* quit_item_p;
  GtkMenuItem* print_log_item_p;
  GtkMenuItem* page_setup_log_item_p;
  GtkMenuItem* view_log_item_p;
  GtkMenuItem* about_efax_gtk_item_p;
  GtkMenuItem* about_efax_item_p; 
  GtkMenuItem* translations_item_p;
  GtkMenuItem* help_item_p;
  GtkWidget* pass_entry_p;
  std::string max_text;
  MessageText text_window;
  StatusLine status_line;
  EfaxController efax_controller;
  SocketServer socket_server;

  std::pair<std::string, unsigned int> notified_fax;
  std::string selected_socket_list_file;

  FaxListDialog* received_fax_list_p;
  FaxListDialog* sent_fax_list_p;
  SocketListDialog* socket_list_p;
  HelpDialog* helpfile_p;

  TrayItem tray_item;

  Utf8::Reassembler error_pipe_reassembler;

  GobjHandle<GdkGC> red_gc_h;

  // these are called by handlers of GTK+ signals
  void close_impl(void);
  void get_file_impl(void);
  void file_list_impl(void);
  void socket_list_impl(void);
  void addressbook_impl(void);
  void receive_impl(EfaxController::State);
  void fax_list_impl(FaxListEnum::Mode);
  void settings_impl(void);
  void about_impl(bool);
  void translations_impl(void);
  void helpfile_impl(void);
  void set_file_items_sensitive_impl(void);
  void set_socket_items_sensitive_impl(void);
  void style_set_impl(void);

  // these are connected to sigc signals or Notifier objects, or connected
  // to the callback of a WatchSource (iowatch) object, and might also be
  // called by the handlers of GTK+ signals
  void sendfax_slot(void);
  void set_files_slot(const std::string&);
  void fax_received_notify_slot(const std::pair<std::string, std::string>&);
  void enter_socket_file_slot(const std::pair<std::string, std::string>&);
  void settings_changed_slot(const std::string&);
  void socket_filelist_changed_notify_slot(void);
  void socket_filelist_closed_slot(void);
  void fax_to_send_notify_slot(void);
  void set_number_slot(const std::string&);
  void tray_icon_menu_slot(int);
  void tray_icon_left_clicked_slot(void);
  void quit_slot(void);
  void present_window_slot();
  bool read_error_pipe_slot(void);

  void fax_to_send_dialog(const std::pair<std::string, unsigned int>&);
  void get_longest_button_text(void);
  bool draw_fax_from_socket_notifier(void);
  void get_window_settings(void);
  void save_window_settings(void);
  void strip(std::string&);
  void start_pipe_thread(void);
  void pipe_thread(void);
protected:
  virtual void on_delete_event(void);
public:
  friend void MainWindowCB::mainwin_button_clicked(GtkWidget*, void*);
  friend void MainWindowCB::mainwin_menuitem_activated(GtkMenuItem*, void*);
  friend gboolean MainWindowCB::mainwin_key_press_event(GtkWidget*, GdkEventKey*, void*);
  friend gboolean MainWindowCB::mainwin_visibility_notify_event(GtkWidget*,
								GdkEventVisibility*, void*);
  friend gboolean MainWindowCB::mainwin_window_state_event(GtkWidget*,
							   GdkEventWindowState*, void*);
  friend void MainWindowCB::mainwin_style_set(GtkWidget*, GtkStyle*, void*);
  friend gboolean MainWindowCB::mainwin_timer_event(void*);
  friend gboolean MainWindowCB::mainwin_drawing_area_expose_event(GtkWidget*,
								  GdkEventExpose*, void*);
  friend gboolean MainWindowCB::mainwin_start_hidden_check(void*);

  friend ssize_t write_error(const char*);
  friend int connect_to_stderr(void);

  void remove_from_socket_server_filelist(const std::string& file);
  MainWindow(const std::string&, bool start_hidden = false,
	     bool start_in_standby = false, const char* filename = 0);
  ~MainWindow(void);
};

#endif
