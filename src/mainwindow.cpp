/* Copyright (C) 2001 to 2007 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

// something in gtkprintjob.h clashes with the C++ headers with
// libstdc++-5 and it needs to be included here to obtain a
// clean compile
#include <gtk/gtkversion.h>
#if GTK_CHECK_VERSION(2,10,0)
#include <gtk/gtkprintjob.h>
#endif

#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <fstream>
#include <memory>
#include <cstdlib>
#include <cstring>

#ifdef HAVE_OSTREAM
#include <ios>
#include <ostream>
#endif

#ifdef HAVE_ISTREAM
#include <ios>
#include <istream>
#endif

#ifdef HAVE_STREAM_IMBUE
#include <locale>
#endif

#include <gtk/gtkcontainer.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktexttagtable.h>
#include <gtk/gtktextiter.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtksettings.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkmain.h>

#include <pango/pango-layout.h>

#include <gdk/gdkcolor.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkdrawable.h>
#include <gdk/gdkpixbuf.h>
#include <gdk/gdkkeysyms.h> // the key codes are here

#include <glib/glist.h>
#include <glib/gmain.h>

#if GTK_CHECK_VERSION(2,4,0)
#  define EFAX_GTK_USE_ICON_THEME 1
#else
#  undef EFAX_GTK_USE_ICON_THEME
#endif

#ifdef EFAX_GTK_USE_ICON_THEME
#include <gtk/gtkicontheme.h>
#include "utils/icon_info_handle.h"
#endif

#if GTK_CHECK_VERSION(2,10,0)
#  define EFAX_GTK_USE_GTK_PRINT 1
#else
#  undef EFAX_GTK_USE_GTK_PRINT
#endif

#include "mainwindow.h"
#include "dialogs.h"
#include "addressbook.h"
#include "file_list.h"
#include "settings.h"
#include "socket_notify.h"
#include "menu_icons.h"
#include "utils/shared_handle.h"
#include "utils/thread.h"
#include "utils/fdstream.h"
#include "utils/gerror_handle.h"
#include "utils/callback.h"



#ifdef ENABLE_NLS
#include <libintl.h>
#endif


#define TIMER_INTERVAL 200  // number of milliseconds between timer events for the main timer

extern "C" void close_signalhandler(int);
static volatile sig_atomic_t close_flag = false;



static Thread::Mutex* write_error_mutex_p = 0;
bool MainWindow::connected_to_stderr = false;
PipeFifo MainWindow::error_pipe(PipeFifo::non_block);


namespace { // callbacks for internal use only

void MainWindowCB::mainwin_button_clicked(GtkWidget* widget_p, void* data) {
  MainWindow* instance_p = static_cast<MainWindow*>(data);

  if (widget_p == instance_p->file_button_p) {
    instance_p->set_file_items_sensitive_impl();
  }
  else if (widget_p == instance_p->socket_button_p) {
    instance_p->set_socket_items_sensitive_impl();
  }
  else if (widget_p == instance_p->single_file_button_p) {
    instance_p->get_file_impl();
  }
  /*else if (widget_p == instance_p->multiple_file_button_p) {
    instance_p->file_list_impl();
  }*/
  else if (widget_p == instance_p->socket_list_button_p) {
    instance_p->socket_list_impl();
  }
  else if (widget_p == instance_p->number_button_p) {
    instance_p->addressbook_impl();
  }
  else if (widget_p == instance_p->send_button_p) {
    instance_p->sendfax_slot();
  }
  else if (widget_p == instance_p->receive_answer_button_p) {
    instance_p->receive_impl(EfaxController::receive_answer);
  }
  /*else if (widget_p == instance_p->receive_takeover_button_p) {
    instance_p->receive_impl(EfaxController::receive_takeover);
  }*/
  else if (widget_p == instance_p->receive_standby_button_p) {
    instance_p->receive_impl(EfaxController::receive_standby);
  }
  else if (widget_p == instance_p->stop_button_p) {
    instance_p->efax_controller.stop();
  }
  else {
    write_error("Callback error in MainWindowCB::mainwin_button_clicked()\n");
    return;
  }
}

void MainWindowCB::mainwin_menuitem_activated(GtkMenuItem* item_p, void* data) {
  MainWindow* instance_p = static_cast<MainWindow*>(data);
  if (item_p == instance_p->list_received_faxes_item_p) {
    instance_p->fax_list_impl(FaxListEnum::received);
  }
  else if (item_p == instance_p->list_sent_faxes_item_p) {
    instance_p->fax_list_impl(FaxListEnum::sent);
  }
  else if (item_p == instance_p->socket_list_item_p) {
    instance_p->socket_list_impl();
  }
  else if (item_p == instance_p->single_file_item_p) {
    instance_p->get_file_impl();
  }
  /*else if (item_p == instance_p->multiple_file_item_p) {
    instance_p->file_list_impl();
  }*/
  else if (item_p == instance_p->address_book_item_p) {
    instance_p->addressbook_impl();
  }
  else if (item_p == instance_p->settings_item_p) {
    instance_p->settings_impl();
  }
  else if (item_p == instance_p->quit_item_p) {
    instance_p->close_impl();
  }
  else if (instance_p->print_log_item_p && item_p == instance_p->print_log_item_p) {
    instance_p->text_window.print_log();
  }
  else if (instance_p->page_setup_log_item_p && item_p == instance_p->page_setup_log_item_p) {
    MessageText::print_page_setup(instance_p->get_win());
  }
  else if (item_p == instance_p->view_log_item_p) {
    instance_p->text_window.view_log(instance_p->standard_size);
  }
  else if (item_p == instance_p->about_efax_gtk_item_p) {
    instance_p->about_impl(true);
  }
  else if (item_p == instance_p->about_efax_item_p) {
    instance_p->about_impl(false);
  }
  else if (item_p == instance_p->translations_item_p) {
    instance_p->translations_impl();    
  }
  else if (item_p == instance_p->help_item_p) {
    instance_p->helpfile_impl();
  }
  else {
    write_error("Callback error in MainWindowCB::mainwin_menuitem_activated()\n");
    return;
  }
}

gboolean MainWindowCB::mainwin_key_press_event(GtkWidget*, GdkEventKey* event_p, void* data) {
  MainWindow* instance_p = static_cast<MainWindow*>(data);
  
  if (event_p->keyval == GDK_F1) {
    instance_p->helpfile_impl();
    return true; // processing stops here
  }
  return false;  // carry on processing the key event
}

gboolean MainWindowCB::mainwin_visibility_notify_event(GtkWidget*, GdkEventVisibility* event_p,
						       void* data) {
  MainWindow* instance_p = static_cast<MainWindow*>(data);

  if(event_p->state == GDK_VISIBILITY_FULLY_OBSCURED) instance_p->obscured = true;
  else instance_p->obscured = false;

  return false;  // carry on processing the visibility notify event
}

gboolean MainWindowCB::mainwin_window_state_event(GtkWidget*, GdkEventWindowState* event_p,
						  void* data) {
  MainWindow* instance_p = static_cast<MainWindow*>(data);

  if(event_p->new_window_state == GDK_WINDOW_STATE_ICONIFIED) instance_p->minimised = true;
  else instance_p->minimised = false;

  return false;  // carry on processing the window state event
}

void MainWindowCB::mainwin_style_set(GtkWidget*, GtkStyle*, void* data) {
  static_cast<MainWindow*>(data)->style_set_impl();
}

gboolean MainWindowCB::mainwin_timer_event(void* data) {
  MainWindow* instance_p = static_cast<MainWindow*>(data);

  if (close_flag) {
    instance_p->close_impl(); // we must have picked up an external kill signal
                              // so we need an orderly close down
    close_flag = false;
  }
  instance_p->efax_controller.timer_event();
  return true; // we want a multi-shot timer
}

gboolean MainWindowCB::mainwin_drawing_area_expose_event(GtkWidget*,
							 GdkEventExpose*,
							 void* data) {
  return static_cast<MainWindow*>(data)->draw_fax_from_socket_notifier();
}

gboolean MainWindowCB::mainwin_start_hidden_check(void* data) {
  MainWindow* instance_p = static_cast<MainWindow*>(data);
  
  if (!instance_p->tray_item.is_embedded()) {
    // the user has goofed - he has set the program to start hidden in the system tray
    // but has no system tray running!
    write_error(gettext("The program has been started with the -s option but there is no system tray!\n"));
    gtk_window_present(instance_p->get_win());
  }
  return false; // this only fires once
}

} // anonymous namespace


MainWindow::MainWindow(const std::string& messages,bool start_hidden,
		       bool start_in_standby, const char* filename):
                       // start_hidden and start_in_standby have a default value of
		       // false and filename has a default value of 0
			      WinBase(0, prog_config.window_icon_h, false),
                              standard_size(24),
			      obscured(false),
			      minimised(false),
			      text_window(WinBase::get_win()),
                              status_line(standard_size),
			      tray_item(WinBase::get_win()) {

  // catch any relevant Unix signals for an orderly closedown
  // catch SIGQUIT, SIGTERM SIGINT SIGHUP
  // it is safe to use signal() for these
  signal(SIGQUIT, close_signalhandler);
  signal(SIGTERM, close_signalhandler);
  signal(SIGINT, close_signalhandler);
  signal(SIGHUP, close_signalhandler);

  // ignore SIGPIPE
  struct sigaction sig_act_pipe;
  sig_act_pipe.sa_handler = SIG_IGN;
  // we don't need to mask off any signals
  sigemptyset(&sig_act_pipe.sa_mask);
  sig_act_pipe.sa_flags = 0;
  sigaction(SIGPIPE, &sig_act_pipe, 0);

  // we don't need to set a child signal handler - the default
  // (SIG_DFL) is fine - we want to ignore it as we will reap
  // exit status in EfaxController::timer_event(), which calls
  // waitpid()

  write_error_mutex_p = new Thread::Mutex;

  get_longest_button_text();

  GtkWidget* drawing_area_alignment_p = gtk_alignment_new(1, 0.5, 0, 1);
  drawing_area_p = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing_area_p, standard_size, standard_size);
  gtk_container_add(GTK_CONTAINER(drawing_area_alignment_p),
		    drawing_area_p);

  GtkTable* file_entry_table_p = GTK_TABLE(gtk_table_new(3, 3, false));
  GtkTable* win_table_p = GTK_TABLE(gtk_table_new(5, 3, false));

  file_button_p = gtk_radio_button_new_with_label(0, gettext("File  "));
  /*socket_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(file_button_p),
							gettext(""));*/

  GtkWidget* fax_method_label_p = gtk_label_new(gettext("Fax entry method: "));
  GtkWidget* pass_entry_label_p = gtk_label_new(gettext("Passphrase : "));
  single_file_button_p = gtk_button_new_with_label(gettext("Scan File"));
//  multiple_file_button_p = gtk_button_new_with_label(gettext("Multiple files"));
  socket_list_button_p = gtk_button_new_with_label(gettext(""));
  number_button_p = gtk_button_new_with_label(gettext("Tel number: "));
  send_button_p = gtk_button_new_with_label(gettext("Send fax"));
  receive_answer_button_p = gtk_button_new_with_label(gettext("Answer call"));
 // receive_takeover_button_p = gtk_button_new_with_label(gettext("Take over call"));
  receive_standby_button_p = gtk_button_new_with_label(gettext("Standby"));
  stop_button_p = gtk_button_new_with_label(gettext("Stop"));

  file_entry_p = gtk_entry_new();
  number_entry_p = gtk_entry_new();
  pass_entry_p= gtk_entry_new();

  GtkBox* fax_method_radio_box_p = GTK_BOX(gtk_hbox_new(false, 0));
  gtk_box_pack_start(fax_method_radio_box_p, file_button_p,
		     false, false, 0);
  gtk_box_pack_start(fax_method_radio_box_p, socket_button_p,
		     false, false, 0);
  GtkWidget* fax_method_radio_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(fax_method_radio_frame_p),
		    GTK_WIDGET(fax_method_radio_box_p));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(file_button_p), true);

  GtkBox* fax_method_box_p = GTK_BOX(gtk_hbox_new(false, 0));
  gtk_box_pack_start(fax_method_box_p, fax_method_label_p,
		     false, false, 0);
  gtk_box_pack_start(fax_method_box_p, fax_method_radio_frame_p,
		     false, false, 0);
  gtk_box_pack_start(fax_method_box_p, drawing_area_alignment_p,
		     true, true, 0);

  gtk_table_attach(file_entry_table_p, file_entry_p,
		   0, 3, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);
  gtk_table_attach(file_entry_table_p, GTK_WIDGET(fax_method_box_p),
		   0, 3, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);
  gtk_table_attach(file_entry_table_p, single_file_button_p,
		   1, 2, 1, 3,
		   GTK_EXPAND, GTK_SHRINK,
		   standard_size/3, standard_size/3);
/*  gtk_table_attach(file_entry_table_p, multiple_file_button_p,
		   1, 2, 2, 3,
		   GTK_EXPAND, GTK_SHRINK,
                   standard_size/3, standard_size/3);*/
 /* gtk_table_attach(file_entry_table_p, socket_list_button_p,
		   2, 3, 2, 3,
		   GTK_EXPAND, GTK_SHRINK,
		   standard_size/3, standard_size/3);*/

  GtkWidget* file_entry_frame_p = gtk_frame_new(gettext("Fax to send"));
  gtk_container_add(GTK_CONTAINER(file_entry_frame_p),
		    GTK_WIDGET(file_entry_table_p));

  GtkBox* number_box_p = GTK_BOX(gtk_hbox_new(false, 0));
  gtk_box_pack_start(number_box_p, number_button_p,
		     false, false, standard_size/2);
  gtk_box_pack_start(number_box_p, number_entry_p,true, true, 0);
  gtk_box_pack_start(number_box_p, pass_entry_label_p,true, true, 0);
  gtk_box_pack_start(number_box_p, pass_entry_p,true, true, 0);

  gtk_table_attach(win_table_p, file_entry_frame_p,
		   0, 3, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);
  gtk_table_attach(win_table_p, GTK_WIDGET(number_box_p),
		   0, 3, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);
  gtk_table_attach(win_table_p, text_window.get_main_widget(),
		   0, 3, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/3, standard_size/3);
  
  gtk_table_attach(win_table_p, send_button_p,
		   0, 1, 3, 4,
		   GTK_EXPAND, GTK_SHRINK,
		   standard_size/3, standard_size/3);
  gtk_table_attach(win_table_p, receive_answer_button_p,
		   2, 3, 3, 4,
		   GTK_EXPAND, GTK_SHRINK,
		   standard_size/3, standard_size/3);
 /* gtk_table_attach(win_table_p, receive_takeover_button_p,
		   2, 3, 3, 4,
		   GTK_EXPAND, GTK_SHRINK,
                   standard_size/3, standard_size/3);*/

  gtk_table_attach(win_table_p, receive_standby_button_p,
		   0, 1, 4, 5,
		   GTK_EXPAND, GTK_SHRINK,
		   standard_size/3, standard_size/3);
  gtk_table_attach(win_table_p, stop_button_p,
		   2, 3, 4, 5,
		   GTK_EXPAND, GTK_SHRINK,
		   standard_size/3, standard_size/3);
  
  g_signal_connect(G_OBJECT(file_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);
  /*g_signal_connect(G_OBJECT(socket_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);*/
  g_signal_connect(G_OBJECT(single_file_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);
/*  g_signal_connect(G_OBJECT(multiple_file_button_p), "clicked",
                   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);*/
  g_signal_connect(G_OBJECT(socket_list_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);
  g_signal_connect(G_OBJECT(number_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);
  g_signal_connect(G_OBJECT(send_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);
  g_signal_connect(G_OBJECT(receive_answer_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);
 /* g_signal_connect(G_OBJECT(receive_takeover_button_p), "clicked",
                   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);*/
  g_signal_connect(G_OBJECT(receive_standby_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);
  g_signal_connect(G_OBJECT(stop_button_p), "clicked",
		   G_CALLBACK(MainWindowCB::mainwin_button_clicked), this);

  GTK_WIDGET_SET_FLAGS(single_file_button_p, GTK_CAN_DEFAULT);
  //GTK_WIDGET_SET_FLAGS(multiple_file_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(socket_list_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(number_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(send_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(receive_answer_button_p, GTK_CAN_DEFAULT);
  //GTK_WIDGET_SET_FLAGS(receive_takeover_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(receive_standby_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(stop_button_p, GTK_CAN_DEFAULT);

  // set up the menu bar
  // first get the about icon

  bool have_about_icon = false;
  GobjHandle<GdkPixbuf> about_pixbuf_h;

#ifdef EFAX_GTK_USE_ICON_THEME

  GtkIconTheme* icon_theme_p = gtk_icon_theme_get_default();

  IconInfoScopedHandle icon_info_h(gtk_icon_theme_lookup_icon(icon_theme_p,
							      "stock_about",
							      16, GtkIconLookupFlags(0)));
  if (icon_info_h.get()) {

    const gchar* icon_path_p = gtk_icon_info_get_filename(icon_info_h.get());
    if (icon_path_p) {
      GError* error_p = 0;
      about_pixbuf_h = GobjHandle<GdkPixbuf>(gdk_pixbuf_new_from_file(icon_path_p, &error_p));
      if (about_pixbuf_h.get()) have_about_icon = true;
      else {
	write_error("Pixbuf error in MainWindow::MainWindow()\n");
	if (error_p) {
	  GerrorScopedHandle handle_h(error_p);
	  write_error(error_p->message);
	  write_error("\n");
	}
      }
    }
  }
#endif

  if (!have_about_icon) {
    about_pixbuf_h = GobjHandle<GdkPixbuf>(gdk_pixbuf_new_from_xpm_data(about_xpm));
  }

  // create the file menu
  GtkWidget* file_menu_p = gtk_menu_new();
  GtkWidget* image_p;

  image_p = gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU);
  list_received_faxes_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("List _received faxes")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(list_received_faxes_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(list_received_faxes_item_p));
  gtk_widget_show(GTK_WIDGET(list_received_faxes_item_p));
  
  image_p = gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU);
  list_sent_faxes_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("_List sent faxes")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(list_sent_faxes_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(list_sent_faxes_item_p));
  gtk_widget_show(GTK_WIDGET(list_sent_faxes_item_p));

  // insert separator
  GtkWidget* separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), separator_item_p);
  gtk_widget_show(separator_item_p);

  socket_list_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(gettext("Queued _faxes from socket")));
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(socket_list_item_p));
  gtk_widget_show(GTK_WIDGET(socket_list_item_p));

  image_p = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
  single_file_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("_Enter single file")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(single_file_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(single_file_item_p));
  gtk_widget_show(GTK_WIDGET(single_file_item_p));

  image_p = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
/*  multiple_file_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("Enter _multiple files")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(multiple_file_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(multiple_file_item_p));
  gtk_widget_show(GTK_WIDGET(multiple_file_item_p));*/

  // insert separator
  separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), separator_item_p);
  gtk_widget_show(separator_item_p);

  address_book_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(gettext("_Address book")));
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(address_book_item_p));
  gtk_widget_show(GTK_WIDGET(address_book_item_p));

  // insert separator
  separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), separator_item_p);
  gtk_widget_show(separator_item_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
  settings_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("_Settings")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(settings_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(settings_item_p));
  gtk_widget_show(GTK_WIDGET(settings_item_p));

  // insert separator
  separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), separator_item_p);
  gtk_widget_show(separator_item_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU);
  quit_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("_Quit")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(quit_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu_p), GTK_WIDGET(quit_item_p));
  gtk_widget_show(GTK_WIDGET(quit_item_p));


  // create the log menu
  GtkWidget* log_menu_p = gtk_menu_new();

#if EFAX_GTK_USE_GTK_PRINT
  image_p = gtk_image_new_from_stock(GTK_STOCK_PRINT, GTK_ICON_SIZE_MENU);
  print_log_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("_Print logfile")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(print_log_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(log_menu_p), GTK_WIDGET(print_log_item_p));
  gtk_widget_show(GTK_WIDGET(print_log_item_p));

  image_p = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
  page_setup_log_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("Print logfile page _setup")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(page_setup_log_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(log_menu_p), GTK_WIDGET(page_setup_log_item_p));
  gtk_widget_show(GTK_WIDGET(page_setup_log_item_p));
#else
  print_log_item_p = 0;
  page_setup_log_item_p = 0;
#endif

  { // scope block for the GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(view_xpm));
    pixbuf_h = GobjHandle<GdkPixbuf>(gdk_pixbuf_scale_simple(pixbuf_h, 16, 16, GDK_INTERP_BILINEAR));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    view_log_item_p =
      GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("_View logfile")));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(view_log_item_p), image_p);
    gtk_menu_shell_append(GTK_MENU_SHELL(log_menu_p), GTK_WIDGET(view_log_item_p));
    gtk_widget_show(GTK_WIDGET(view_log_item_p));
  }

  if (prog_config.logfile_name.empty()) {
    gtk_widget_set_sensitive(GTK_WIDGET(view_log_item_p), false);
#if EFAX_GTK_USE_GTK_PRINT
    gtk_widget_set_sensitive(GTK_WIDGET(print_log_item_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(page_setup_log_item_p), false);
#endif
  }

  // create the help menu
  GtkWidget* help_menu_p = gtk_menu_new();

  image_p = gtk_image_new_from_pixbuf(about_pixbuf_h);
  about_efax_gtk_item_p = 
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("About efax-_gtk")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(about_efax_gtk_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu_p), GTK_WIDGET(about_efax_gtk_item_p));
  gtk_widget_show(GTK_WIDGET(about_efax_gtk_item_p));
    
  image_p = gtk_image_new_from_pixbuf(about_pixbuf_h);
  about_efax_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("About _efax")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(about_efax_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu_p), GTK_WIDGET(about_efax_item_p));
  gtk_widget_show(GTK_WIDGET(about_efax_item_p));

  translations_item_p = 
    GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(gettext("_Translations")));
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu_p), GTK_WIDGET(translations_item_p));
  gtk_widget_show(GTK_WIDGET(translations_item_p));

  // insert separator
  separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu_p), separator_item_p);
  gtk_widget_show(separator_item_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_HELP, GTK_ICON_SIZE_MENU);
  help_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(gettext("_Help")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(help_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu_p), GTK_WIDGET(help_item_p));
  gtk_widget_show(GTK_WIDGET(help_item_p));

  // now put the menus into a menubar
  GtkWidget* root_file_item_p = gtk_menu_item_new_with_mnemonic(gettext("_File"));
  GtkWidget* root_log_item_p = gtk_menu_item_new_with_mnemonic(gettext("_Log"));
  GtkWidget* root_help_item_p = gtk_menu_item_new_with_mnemonic(gettext("_Help"));
  
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_file_item_p), file_menu_p);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_log_item_p), log_menu_p);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_help_item_p), help_menu_p);

  GtkMenuShell* menu_bar_p = GTK_MENU_SHELL(gtk_menu_bar_new());
  gtk_menu_shell_append(menu_bar_p, root_file_item_p);
  gtk_menu_shell_append(menu_bar_p, root_log_item_p);
  gtk_menu_shell_append(menu_bar_p, root_help_item_p);

  // connect the activate signals
  g_signal_connect(G_OBJECT(list_received_faxes_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(list_sent_faxes_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(socket_list_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(single_file_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
 /* g_signal_connect(G_OBJECT(multiple_file_item_p), "activate",
                   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);*/
  g_signal_connect(G_OBJECT(address_book_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(settings_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(quit_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
#if EFAX_GTK_USE_GTK_PRINT
  g_signal_connect(G_OBJECT(print_log_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(page_setup_log_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
#endif
  g_signal_connect(G_OBJECT(view_log_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(about_efax_gtk_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(about_efax_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(translations_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);
  g_signal_connect(G_OBJECT(help_item_p), "activate",
		   G_CALLBACK(MainWindowCB::mainwin_menuitem_activated), this);

  // collect up everything into the main window vbox
  GtkBox* window_box_p = GTK_BOX(gtk_vbox_new(false, 0));
  gtk_box_pack_start(window_box_p, GTK_WIDGET(menu_bar_p),
		     false, false, 0);
  gtk_box_pack_start(window_box_p, GTK_WIDGET(win_table_p),
		     true, true, 0);
  gtk_box_pack_start(window_box_p, status_line.get_main_widget(),
		     false, false, 0);
  
  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(window_box_p));

  // connect up miscellaneous signals for the GTK+ main window object
  g_signal_connect(G_OBJECT(get_win()), "key_press_event",
		   G_CALLBACK(MainWindowCB::mainwin_key_press_event), this);
  g_signal_connect(G_OBJECT(get_win()), "visibility_notify_event",
		   G_CALLBACK(MainWindowCB::mainwin_visibility_notify_event), this);
  g_signal_connect(G_OBJECT(get_win()), "window_state_event",
		   G_CALLBACK(MainWindowCB::mainwin_window_state_event), this);
  g_signal_connect(G_OBJECT(get_win()), "style_set",
		   G_CALLBACK(MainWindowCB::mainwin_style_set), this);

  gtk_container_set_border_width(GTK_CONTAINER(win_table_p), standard_size/3);

  GList* focus_chain_p = 0;
  // prepend in reverse order
  focus_chain_p = g_list_prepend(focus_chain_p, stop_button_p);
  focus_chain_p = g_list_prepend(focus_chain_p, receive_standby_button_p);
  //focus_chain_p = g_list_prepend(focus_chain_p, receive_takeover_button_p);
  focus_chain_p = g_list_prepend(focus_chain_p, receive_answer_button_p);
  focus_chain_p = g_list_prepend(focus_chain_p, send_button_p);
  focus_chain_p = g_list_prepend(focus_chain_p, number_box_p);
  focus_chain_p = g_list_prepend(focus_chain_p, file_entry_frame_p);
 
  gtk_container_set_focus_chain(GTK_CONTAINER(win_table_p), focus_chain_p);
  g_list_free(focus_chain_p);

  g_timeout_add(TIMER_INTERVAL, MainWindowCB::mainwin_timer_event, this);

  g_signal_connect(G_OBJECT(drawing_area_p), "expose_event",
		   G_CALLBACK(MainWindowCB::mainwin_drawing_area_expose_event),
		   this);

  efax_controller.ready_to_quit_notify.connect(sigc::mem_fun(*this, &MainWindow::quit_slot));

  efax_controller.stdout_message.connect(sigc::mem_fun(text_window, &MessageText::write_black_slot));
  efax_controller.write_state.connect(sigc::mem_fun(status_line, &StatusLine::write_status_slot));
  efax_controller.remove_from_socket_server_filelist.connect(sigc::mem_fun(*this,
			    &MainWindow::remove_from_socket_server_filelist));

  // connect up our end of the error pipe
  error_pipe_tag = start_iowatch(error_pipe.get_read_fd(),
				 sigc::mem_fun(*this, &MainWindow::read_error_pipe_slot),
				 G_IO_IN);
  // we want to minimise the effect on efax, so make writing to the error pipe non-blocking
  error_pipe.make_write_non_block();

  set_file_items_sensitive_impl();

  get_window_settings();
		    
  // don't have entry widgets selecting on focus
  GtkSettings* settings_p = gtk_settings_get_default();
  if (settings_p) {
    g_object_set(settings_p,
		 "gtk-entry-select-on-focus", false,
		 static_cast<void*>(0));
  }
  else write_error("Can't obtain default GtkSettings object\n");

  if (filename) {
    // this will also call set_file_items_sensitive_impl() if the call to
    // get_window_settings() has set socket_button active
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(file_button_p), true);
    gtk_entry_set_text(GTK_ENTRY(file_entry_p), filename);
    gtk_window_set_focus(get_win(), number_entry_p);
  }
  else gtk_window_set_focus(get_win(), single_file_button_p);

  // enable visibility events, so that the action when left clicking on the
  // tray icon is correct
  GtkWidget* mainwin_p = GTK_WIDGET(get_win());
  gtk_widget_add_events(mainwin_p, GDK_VISIBILITY_NOTIFY_MASK);

  // now we will either show everything, or realise the window
  // and its children and keep it hidden in the system tray
  if (start_hidden) {
    gtk_widget_realize(mainwin_p);
    gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(get_win())));
    gtk_widget_hide(mainwin_p);
    g_timeout_add(5000, MainWindowCB::mainwin_start_hidden_check, this);
  }
  else gtk_widget_show_all(mainwin_p);

  // now we have shown ourselves (if we are going to show ourselves!)

  if (!messages.empty()) {
    text_window.write_red_slot(messages.c_str());
    text_window.write_red_slot("\n\n");
  }

  // make sure that we have the faxin, faxout, and faxsent
  chdir(prog_config.working_dir.c_str());
  mkdir("faxin", S_IRUSR | S_IWUSR | S_IXUSR);
  mkdir("faxout", S_IRUSR | S_IWUSR | S_IXUSR);
  mkdir("faxsent", S_IRUSR | S_IWUSR | S_IXUSR);

  // set the working directory for the parent process
  chdir("faxin");

  // if there is no config file installed, then bring up the settings dialog
  if (!prog_config.found_rcfile) {
    // we don't want to use MainWindow::settings_impl(), or the absence of
    // a configuration file will be reported twice -- not a big deal, but ...
    // so pass true as the last parameter to skip detection of settings file
    SettingsDialog dialog(standard_size, get_win(), true);
    dialog.accepted.connect(sigc::mem_fun(*this, &MainWindow::settings_changed_slot));
    dialog.exec();
  }

  // this connects the stdout facilities of SocketServer to MessageText
  socket_server.stdout_message.connect(sigc::mem_fun(text_window, &MessageText::write_black_slot));

  // this connects the SigC object in SocketServer to a method in this class
  // which will update the Socket_list object (if such a dialog is displayed)
  socket_server.filelist_changed_notify.connect(sigc::mem_fun(*this,
			               &MainWindow::socket_filelist_changed_notify_slot));

  // and this connects the SigC object which indicates a fax has been received
  // from the socket for sending to the method which will pop up a "fax to send"
  // notify dialog
  socket_server.fax_to_send_notify.connect(sigc::mem_fun(*this,
					   &MainWindow::fax_to_send_notify_slot));

  // this connects the SigC object which indicates that a fax has been received
  // from the modem by the EfaxController object
  efax_controller.fax_received_notify.connect(sigc::mem_fun(*this,
			                      &MainWindow::fax_received_notify_slot));

  // start the socket server
  if (prog_config.sock_server && !prog_config.sock_server_port.empty()) {
    socket_server.start(std::string(prog_config.sock_server_port),
			prog_config.other_sock_client_address);
  }

  tray_item.left_button_pressed.connect(sigc::mem_fun(*this,
					  &MainWindow::tray_icon_left_clicked_slot));
  tray_item.menu_item_chosen.connect(sigc::mem_fun(*this,
					  &MainWindow::tray_icon_menu_slot));
  tray_item.get_state.connect(sigc::mem_fun(efax_controller, &EfaxController::get_state));
  tray_item.get_new_fax_count.connect(sigc::mem_fun(efax_controller,
						    &EfaxController::get_count));
  efax_controller.write_state.connect(sigc::mem_fun(tray_item, &TrayItem::set_tooltip_slot));

  present_window_notify.connect(sigc::mem_fun(*this, &MainWindow::present_window_slot));
  start_pipe_thread();

  // register our own button icon size for stock icons to match size of externally
  // defined icons used in this program
  gtk_icon_size_register("EFAX_GTK_BUTTON_SIZE", 22, 22);

  // the notifed_fax pair is used by sendfax_slot() if no number is shown
  // to dial, for use if the option to send on an open connection without
  // dialling is refused and sendfax_slot() was called by a
  // SocketNotifyDialog::sendfax_sig signal, so that a SocketNotifyDialog
  // dialog can be brought up again for the user to have another chance to
  // choose what he/she wants to do.  When the second member of the
  // notified_fax pair is not 0, then that indicates that sendfax_slot()
  // was called by a SocketNotifyDialog object (it is normally set to 0 and
  // only holds another value when set in MainWindow::fax_to_send_dialog())
  notified_fax.second = 0;

  // if the -r option has been chosen, start the program in Receive Standby mode
  if (start_in_standby) receive_impl(EfaxController::receive_standby);
}

MainWindow::~MainWindow(void) {
  save_window_settings();
  g_source_remove(error_pipe_tag);
  delete write_error_mutex_p;
}

void MainWindow::get_window_settings(void) {

  int width = 0;
  int height = 0;
  int socket_val = 0;
  int received_list_sort_type = -1;
  int sent_list_sort_type = -1;

  std::string file_name(prog_config.working_dir + "/" MAINWIN_SAVE_FILE);
  std::ifstream file(file_name.c_str(), std::ios::in);

  if (!file) write_error("Can't get mainwindow settings from file\n");
  else {

#ifdef HAVE_STREAM_IMBUE
    file.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

    file >> width >> height >> socket_val
	 >> received_list_sort_type >> sent_list_sort_type;

    if (width > 0 && height > 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(socket_button_p), socket_val);
      gtk_window_set_default_size(get_win(), width, height);
      if ((received_list_sort_type == GTK_SORT_ASCENDING
	   || received_list_sort_type == GTK_SORT_DESCENDING)
	  && (sent_list_sort_type == GTK_SORT_ASCENDING
	      || sent_list_sort_type == GTK_SORT_DESCENDING)) {
	FaxListManager::set_received_list_sort_type(GtkSortType(received_list_sort_type));
	FaxListManager::set_sent_list_sort_type(GtkSortType(sent_list_sort_type));
      }
    }
  }
}

void MainWindow::save_window_settings(void) {

  std::string file_name(prog_config.working_dir + "/" MAINWIN_SAVE_FILE);
  std::ofstream file(file_name.c_str(), std::ios::out);

  if (!file) write_error("Can't save mainwindow settings to file\n");
  else {

#ifdef HAVE_STREAM_IMBUE
    file.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

    int width = 0;
    int height = 0;
    gtk_window_get_size(get_win(), &width, &height);

    int socket_val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(socket_button_p));
    
    file << width << ' '
	 << height << ' '
	 << socket_val << ' '
	 << (int)FaxListManager::get_received_list_sort_type() << ' '
	 << (int)FaxListManager::get_sent_list_sort_type() << ' '
	 << std::endl;
  }
}

void MainWindow::on_delete_event(void) {
  if (tray_item.is_embedded()) gtk_widget_hide(GTK_WIDGET(get_win()));
  else close_impl();
}

void MainWindow::close_impl(void) {

  // close down socket server
  socket_server.stop();
  // make sure we close down efax() if it is active
  // (the EfaxController::ready_to_quit_notify signal is connected to
  // quit_slot() below and will end the main program loop once any efax
  // session running has been dealt with)
  efax_controller.efax_closedown();
}

void MainWindow::quit_slot(void) {
  gtk_main_quit();
}

void MainWindow::sendfax_slot(void) {

  std::string number_entry(gtk_entry_get_text(GTK_ENTRY(number_entry_p)));
  std::string passp_entry(gtk_entry_get_text(GTK_ENTRY(pass_entry_p)));
  char pass_str[50];
  int len;
  FILE *fp; 
 // eliminate leading or trailing spaces so that we can check for an empty string
  strip(number_entry);  
  if (number_entry.empty()) {
    PromptDialog dialog(gettext("No fax number specified.  Do you want to send the fax "
				"on an open connection?"),
			gettext("Telephone number"), standard_size, get_win());
    if (!dialog.exec()) {
      if (notified_fax.second) {
	// we must have got here by a SocketNotifyDialog::sendfax_sig signal
	set_files_slot("");
	selected_socket_list_file = "";
	fax_to_send_dialog(notified_fax);
      }
      return;
    }
  }

  strip(passp_entry);  
  if (passp_entry.empty()) {
    PromptDialog dialog(gettext("No password specified.  Please specify a password."),gettext("Specify Password"), standard_size, get_win());
    if (!dialog.exec()) {
      return;
    }
    return;
  }
  std::strcpy (pass_str, passp_entry.c_str());
   len=strlen(pass_str);
   fp = fopen( "/home/secfax/Desktop/file.txt" , "w" );
   fwrite(pass_str , 1 , len , fp );

   fclose(fp);
  // Fax_item is defined in efax_controller.h
  Fax_item fax_item;

  try {
    fax_item.number = Utf8::locale_from_utf8(prog_config.dial_prefix);
    fax_item.number += Utf8::locale_from_utf8(number_entry);
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in sendfax_slot()\n");
    beep();
    return;
  }

  notified_fax.second = 0;

  gtk_window_present(get_win());

  std::string files(gtk_entry_get_text(GTK_ENTRY(file_entry_p)));
  // if this is a fax received from the socket and entered via the socket faxes
  // list, then "*** " will form the first part of the "file" displayed in the file
  // entry box - so test for it to see if we are sending a file received via the
  // socket rather than a regular file
  if (!files.empty() && files.substr(0,4) == std::string("*** ")) { 
    fax_item.is_socket_file = true;
    fax_item.file_list.push_back(selected_socket_list_file);
  }

  else {
    fax_item.is_socket_file = false;
    // convert the contents of files to locale filename codeset if necessary
    try {
      files = Utf8::filename_from_utf8(files);
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in sendfax_slot()\n");
      beep();
      return;
    }

    const char* separators = ",;";
    // split the files string into separate file names
    std::string::size_type pos1, pos2;
    pos1 = files.find_first_not_of(separators);
    while (pos1 != std::string::npos) {
      pos2 = files.find_first_of(separators, pos1);
      if (pos2 != std::string::npos) {
	std::string file_item(files.substr(pos1, pos2 - pos1));
	strip(file_item);
	fax_item.file_list.push_back(file_item);
	pos1 = files.find_first_not_of(separators, pos2);
      }
      else {
	std::string file_item(files.substr(pos1));
	strip(file_item);
	fax_item.file_list.push_back(file_item);
	pos1 = std::string::npos;
      }
    }  
  }

  if (!prog_config.found_rcfile) {
    text_window.write_red_slot("Can't send fax -- no efax-gtkrc configuration file found\n\n");
    beep();
  }
  
  else if (prog_config.lock_file.empty()) { // if there is no lock file, then it means no
                                            // serial device has been specified (the lock
                                            // file dir defaults to /var/lock)
    text_window.write_red_slot("Can't send fax -- no valid serial device specified\n\n");
    beep();
  }

  else if (fax_item.file_list.empty()) beep();

  else efax_controller.sendfax(fax_item);
}

void MainWindow::receive_impl(EfaxController::State mode) {
FILE *fp;
int len;
char pass_str[50];
std::string passp_entry(gtk_entry_get_text(GTK_ENTRY(pass_entry_p)));
  if (!prog_config.found_rcfile) {
    text_window.write_red_slot("Can't receive fax -- no efax-gtkrc configuration file found\n\n");
    beep();
  }
  
  else if (prog_config.lock_file.empty()) { // if there is no lock file, then it means no
                                            // serial device has been specified (the lock
                                            // file dir defaults to /var/lock)
    text_window.write_red_slot("Can't receive fax -- no valid serial device specified\n\n");
    beep();
  }

  else 
{
  strip(passp_entry);  
  if (passp_entry.empty()) {
    PromptDialog dialog(gettext("No password specified.  Please specify a password."),gettext("Specify Password"), standard_size, get_win());
    if (!dialog.exec()) {
      return;
    }return;
    efax_controller.stop();
  }
  std::strcpy (pass_str, passp_entry.c_str());
   len=strlen(pass_str);
   fp = fopen( "/home/secfax/Desktop/file.txt" , "w" );
   fwrite(pass_str , 1 , len , fp );
   fclose(fp);
efax_controller.receive(mode);
}
}

void MainWindow::get_file_impl(void) {   
   char command1[100],command2[100];
   //std::string file_loc;
   const char* file_loc[] = {"/home/secfax/scan.pdf", "02", "03", "04"};
 std::vector<std::string> file_result(file_loc, file_loc + 4);
   strcpy(command1,"scanimage --mode Gray --format=tiff> /home/secfax/scan.tiff");
   system(command1);
   strcpy(command2,"tiff2pdf -o /home/secfax/scan.pdf /home/secfax/scan.tiff");
 // FileReadSelectDialog file_dialog(standard_size, false, get_win());
  //file_dialog.exec();
  system(command2);
 // file_loc="/home/secfax/scan.pdf";
  //std::vector<std::string> file_result = file_dialog.get_result();
   //std::vector<std::string> file_result;
  // file_result[0] = file_loc;
  if (!file_result.empty()) {
    set_files_slot(file_result[0]);
    gtk_widget_grab_focus(number_entry_p);
}
}

void MainWindow::set_files_slot(const std::string& files) {
  gtk_entry_set_text(GTK_ENTRY(file_entry_p), files.c_str());
}

void MainWindow::fax_list_impl(FaxListEnum::Mode mode) {

  if (mode == FaxListEnum::received) {

    // because FaxListManager::populate_fax_list() calls gtk_main_iteration()
    // we could get back here again before the constructor of FaxListDialog
    // has been entered, so check the guard in FaxListManager
    if (!FaxListManager::is_fax_received_list_main_iteration()) {

      if (!FaxListDialog::get_is_fax_received_list()) {
	received_fax_list_p = new FaxListDialog(mode, standard_size);
	efax_controller.fax_received_notify.connect(sigc::mem_fun(*received_fax_list_p,
						       &FaxListDialog::insert_new_fax_slot));
	received_fax_list_p->get_new_fax_count_sig.connect(sigc::mem_fun(efax_controller,
						       &EfaxController::get_count));
	received_fax_list_p->reset_sig.connect(sigc::mem_fun(efax_controller,
						       &EfaxController::reset_count));
	// now display the correct number of new received faxes
	received_fax_list_p->display_new_fax_count();
      }
      else gtk_window_present(received_fax_list_p->get_win());
    }
  }
  else if (mode == FaxListEnum::sent) {

    // because FaxListManager::populate_fax_list() calls gtk_main_iteration()
    // we could get back here again before the constructor of FaxListDialog
    // has been entered, so check the guard in FaxListManager
    if (!FaxListManager::is_fax_sent_list_main_iteration()) {

      if (!FaxListDialog::get_is_fax_sent_list()) {
	sent_fax_list_p = new FaxListDialog(mode, standard_size);
	efax_controller.fax_sent_notify.connect(sigc::mem_fun(*sent_fax_list_p,
						       &FaxListDialog::insert_new_fax_slot));
      }
      else gtk_window_present(sent_fax_list_p->get_win());
    }
  }
  // there is no memory leak -- FaxListDialog is modeless and will delete its own memory
  // when it is closed
}

void MainWindow::socket_list_impl(void) {

  // this method will launch the dialog listing queued faxes received from
  // the socket_server (a SocketListDialog object)

  if (!SocketListDialog::get_is_socket_list()) {

    // get the filenames pair - the mutex lock will automatically release when filenames_pair (and
    // so the shared pointer holding the lock) goes out of scope at the end of the if block (we want to keep
    // the shared_ptr alive until after we have set up the connection to update the socket list
    std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> > filenames_pair(socket_server.get_filenames());
    socket_list_p = new SocketListDialog(filenames_pair, standard_size);
    // now connect up the dialog to relevant slots and signals
    update_socket_list_connection = update_socket_list.connect(sigc::mem_fun(*socket_list_p, &SocketListDialog::set_socket_list_rows));
    socket_dialog_connection = close_socket_list_dialog.connect(sigc::mem_fun(*socket_list_p, &SocketListDialog::close_slot));
    socket_list_p->selected_fax.connect(sigc::mem_fun(*this,
			       &MainWindow::enter_socket_file_slot));
    socket_list_p->remove_from_socket_server_filelist.connect(sigc::mem_fun(*this,
			       &MainWindow::remove_from_socket_server_filelist));
    socket_list_p->dialog_closed.connect(sigc::mem_fun(*this,
			       &MainWindow::socket_filelist_closed_slot));

    // there is no memory leak -- SocketListDialog is modeless and will delete its own memory
    // when it is closed
  }
  else gtk_window_present(socket_list_p->get_win());
}

void MainWindow::socket_filelist_changed_notify_slot(void) {

  /*
   This slot calls SocketListDialog::set_socket_list_rows() via the
   MainWindow::update_socket_list signal so updating the socket list
   dialog if that dialog happens to be displayed, and also calls
   gtk_widget_queue_draw() on the drawing_area_p object maintained by
   the MainWindow object to force it to redraw

   This slot is connected to the SocketServer::filelist_changed_notify
   Notifier object

   The SocketServer::filelist_changed_notify signal is emitted by
   SocketServer:read_socket() when a new fax file to be sent is
   received via the socket and also by SocketServer::remove_file().

   SocketServer::remove_file() is called by
   MainWindow::remove_from_socket_server_filelist()

   MainWindow::remove_from_socket_server_filelist() is connected to
   the EfaxController::remove_from_socket_server_filelist signal and
   to the SocketListDialog::remove_from_socket_server_filelist signal

   The EfaxController::remove_from_socket_server_filelist signal is
   emitted by EfaxController::timer_event() when a fax derived from
   the socket list is successfully sent

   The SocketListDialog::remove_from_socket_server_filelist signal is
   emitted by SocketListDialog::remove_file()

   SocketListDialog::remove_file() is connected to the
   PromptDialog::accepted signal (emitted when a user elects to delete
   a queued fax from the list of files received for faxing from the
   socket)
  */

  // If a SocketListDialog object exists, cause it to update its display
  update_socket_list(socket_server.get_filenames());

  // trigger an expose event for the drawing area
  gtk_widget_queue_draw(drawing_area_p);
}

void MainWindow::socket_filelist_closed_slot(void) {

  update_socket_list_connection.disconnect();
  socket_dialog_connection.disconnect();

}

void MainWindow::fax_to_send_notify_slot() {
  fax_to_send_dialog(socket_server.get_fax_to_send());
}

void MainWindow::fax_to_send_dialog(const std::pair<std::string, unsigned int>& fax_pair) {

  if (prog_config.sock_popup) {

    if (GTK_WIDGET_SENSITIVE(GTK_WIDGET(get_win()))
	&& (efax_controller.get_state() == EfaxController::inactive
	    || (efax_controller.get_state() == EfaxController::receive_standby
		&& !efax_controller.is_receiving_fax()))) {
      notified_fax = fax_pair;
      SocketNotifyDialog* dialog_p = new SocketNotifyDialog(standard_size, fax_pair);
      dialog_p->fax_name_sig.connect(sigc::mem_fun(*this,
						&MainWindow::enter_socket_file_slot));
      dialog_p->fax_number_sig.connect(sigc::mem_fun(*this,
						  &MainWindow::set_number_slot));
      dialog_p->sendfax_sig.connect(sigc::mem_fun(*this,
					       &MainWindow::sendfax_slot));
      gtk_window_present(dialog_p->get_win());
    }

    else {
      InfoDialog* dialog_p = new InfoDialog(gettext("A print job has been received on socket"),
					    gettext("efax-gtk socket"),
					    GTK_MESSAGE_INFO,
					    get_win(),
					    !(GTK_WIDGET_SENSITIVE(GTK_WIDGET(get_win()))));
      gtk_window_present(dialog_p->get_win());
    }
  }
  // there is no memory leak - the exec() method has not been called so the dialog
  // is self-owning and will delete itself when it is closed
}

void MainWindow::fax_received_notify_slot(const std::pair<std::string, std::string>& fax_info) {

  if (prog_config.fax_received_exec
      && !prog_config.fax_received_prog.empty()) {

    // get the program name for the exec() call below (because this is a
    // multi-threaded program, we must do this before fork()ing because
    // we use functions to get the argument which is not async-signal-safe)
    std::string cmd;
    try {
      cmd.assign(Utf8::filename_from_utf8(prog_config.fax_received_prog));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in MainWindow::fax_received_notify_slot()\n");
      cmd.assign(prog_config.fax_received_prog);
    }
    // we can use the raw output of cmd.c_str() and fax_info.first.c_str() here
    // - they will remain valid at the execlp() call
    const char* prog_name = cmd.c_str();
    const char* fax_name = fax_info.first.c_str();

    pid_t pid = fork();
    
    if (pid == -1) {
      write_error("Fork error - exiting\n");
      std::exit(FORK_ERROR);
    }
    if (!pid) {  // child process - as soon as everything is set up we are going to do an exec()
      
      // now we have forked, we can connect MainWindow::error_pipe to stderr
      connect_to_stderr();

      // this function requires a null pointer as its last argument but as its
      // latter arguments are passed as an elipsis we need to do an explicit cast
      // for 64 bit systems
      execlp(prog_name, prog_name, fax_name, static_cast<void*>(0));

      // if we reached this point, then the execlp() call must have failed
      // report error and end process - use _exit() and not exit()
      write_error("Can't find the program to execute when a fax is received.\n"
		  "Please check your installation and the PATH environmental variable\n");
      _exit(EXEC_ERROR); 
    } // end of child process
  }

  if (prog_config.fax_received_popup) {
    InfoDialog* dialog_p = new InfoDialog(gettext("A fax has been received by efax-gtk"), 
					  gettext("efax-gtk: fax received"),
					  GTK_MESSAGE_INFO,
					  get_win(),
					  !(GTK_WIDGET_SENSITIVE(GTK_WIDGET(get_win()))));
    gtk_window_present(dialog_p->get_win());
    // there is no memory leak - the exec() method has not been called so the dialog
    // is self-owning and will delete itself when it is closed
  }
}

void MainWindow::enter_socket_file_slot(const std::pair<std::string, std::string>& fax_to_insert) {

  // this slot is connected to the SocketListDialog::selected_fax signal and is
  // called (that is, whenever the send fax button is pressed in a dialog object of
  // that class)

  // it is also connected to the SocketNotifyDialog::fax_name_sig signal, and is called
  // when the send fax button is pressed in a dialog object of that class

  // fax_to_insert.first is the fax label for the fax selected in the SocketListDialog object
  // and fax_to_insert.second is the real (temporary) file name obtained from mkstemp()
 
  // now show the fax label in the "Files to fax" box

  // in case we are called from SocketNotifyDialog::fax_name_sig (when it
  // is possible that the file selection buttons will be active) make
  // sure that the socket button is active
  // (we do not have to close either file selection dialogs, because if
  // they were open when fax_to_send_notify_slot() was called then
  // it would not bring up a SocketNotifyDialog object, and if a file selection
  // dialog was opened up after a SocketNotifyDialog object was brought up,
  // it would render the SocketNotifyDialog inoperative by making its parent
  // (this object) insensitive, so we could not reach here until it is closed)
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(socket_button_p), true);

  // now enter the particulars of the selected file in MainWindow::selected_socket_list_file
  // (we will use this later to send the fax in MainWindow::sendfax_slot())
  selected_socket_list_file = fax_to_insert.second;

  std::string socket_file_label("*** ");
  socket_file_label += fax_to_insert.first;
  socket_file_label += " ***";
  set_files_slot(socket_file_label);
}

void MainWindow::remove_from_socket_server_filelist(const std::string& file) {
  socket_server.remove_file(file);
  // if we have stored the file name for sending, delete it
  // and remove from the "Fax to send" box
  if (file == selected_socket_list_file) {
    selected_socket_list_file = "";
    set_files_slot("");
  }
}

void MainWindow::set_file_items_sensitive_impl(void) {

  gtk_widget_set_sensitive(GTK_WIDGET(single_file_item_p), true);
  //gtk_widget_set_sensitive(GTK_WIDGET(multiple_file_item_p), true);
  gtk_widget_set_sensitive(single_file_button_p, true);
//  gtk_widget_set_sensitive(multiple_file_button_p, true);
  
  gtk_editable_set_editable(GTK_EDITABLE(file_entry_p), true);

  gtk_widget_set_sensitive(GTK_WIDGET(socket_list_item_p), false);
  gtk_widget_set_sensitive(socket_list_button_p, false);

  gtk_window_set_focus(get_win(), single_file_button_p);

  // we now need to close the socket list dialog (if it is open), empty
  // the std::string object holding particulars of the currently selected
  // print job received from the socket server and any print job in the
  // Gtk::Entry object for that print job
  close_socket_list_dialog();
  selected_socket_list_file = "";
  set_files_slot("");
}

void MainWindow::set_socket_items_sensitive_impl(void) {

  gtk_widget_set_sensitive(GTK_WIDGET(single_file_item_p), false);
//  gtk_widget_set_sensitive(GTK_WIDGET(multiple_file_item_p), false);
  gtk_widget_set_sensitive(single_file_button_p, false);
  //gtk_widget_set_sensitive(multiple_file_button_p, false);

  gtk_editable_set_editable(GTK_EDITABLE(file_entry_p), false);

  //gtk_widget_set_sensitive(GTK_WIDGET(socket_list_item_p), true);
  //gtk_widget_set_sensitive(socket_list_button_p, true);
  gtk_widget_set_sensitive(GTK_WIDGET(socket_list_item_p), false);
  gtk_widget_set_sensitive(socket_list_button_p, false);

  gtk_window_set_focus(get_win(), socket_list_button_p);
}

void MainWindow::addressbook_impl(void) {

  if (!AddressBook::get_is_address_list()) {
    AddressBook* dialog_p = new AddressBook(standard_size, get_win());

    dialog_p->accepted.connect(sigc::mem_fun(*this, &MainWindow::set_number_slot));
    // there is no memory leak -- AddressBook will delete its own memory
    // when it is closed
  }
}

void MainWindow::file_list_impl(void) {

  FileListDialog* dialog_p = new FileListDialog(standard_size, get_win());

  dialog_p->accepted.connect(sigc::mem_fun(*this, &MainWindow::set_files_slot));
  // there is no memory leak -- FileListDialog is modeless and will delete its own memory
  // when it is closed
}

void MainWindow::settings_impl(void) {

  if (efax_controller.get_state() != EfaxController::inactive) {
    InfoDialog* dialog_p;
    std::string message(gettext("Can't change settings unless "
				"the program is inactive\n"
				"Press the Stop button to make it inactive"));
    dialog_p = new InfoDialog(message.c_str(), "Change settings",
			      GTK_MESSAGE_WARNING, get_win());

    // there is no memory leak -- exec() was not called so InfoDialog
    // will delete its own memory when it is closed
  }

  else {
    SettingsDialog* dialog_p = new SettingsDialog(standard_size, get_win());

    dialog_p->accepted.connect(sigc::mem_fun(*this, &MainWindow::settings_changed_slot));
    // there is no memory leak -- SettingsDialog will delete its own memory
    // when it is closed
  }
}

void MainWindow::settings_changed_slot(const std::string& messages) {
  text_window.reset_logfile();
  if (!messages.empty()) {
    try {
      text_window.write_red_slot(Utf8::locale_from_utf8(messages).c_str());
      text_window.write_red_slot("\n");
    }
    catch  (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in MainWindow::settings_changed_slot()\n");
    }
  }
  if ((!prog_config.sock_server || prog_config.sock_server_port.empty())
      && socket_server.is_server_running()) {
    socket_server.stop();
  }
  else if (prog_config.sock_server
      && !prog_config.sock_server_port.empty()
      && !socket_server.is_server_running()) {
    socket_server.start(std::string(prog_config.sock_server_port),
			prog_config.other_sock_client_address);
  }
  else if (socket_server.get_port() != std::string(prog_config.sock_server_port)
	   || socket_server.get_other_sock_client_address() != prog_config.other_sock_client_address) {
    socket_server.stop();
    socket_server.start(std::string(prog_config.sock_server_port),
			prog_config.other_sock_client_address);
  }

  if (prog_config.logfile_name.empty()) {
    gtk_widget_set_sensitive(GTK_WIDGET(view_log_item_p), false);
#if EFAX_GTK_USE_GTK_PRINT
    gtk_widget_set_sensitive(GTK_WIDGET(print_log_item_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(page_setup_log_item_p), false);
#endif
  }
  else {
    gtk_widget_set_sensitive(GTK_WIDGET(view_log_item_p), true);
#if EFAX_GTK_USE_GTK_PRINT
    gtk_widget_set_sensitive(GTK_WIDGET(print_log_item_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(page_setup_log_item_p), true);
#endif
  }
}

void MainWindow::helpfile_impl(void) {

  if (!HelpDialog::get_is_helpfile()) {
    helpfile_p = new HelpDialog(standard_size);
  }
  else gtk_window_present(helpfile_p->get_win());

  // there is no memory leak -- HelpDialog is modeless and will delete its own memory
  // when it is closed
}

void MainWindow::set_number_slot(const std::string& number) {

  gtk_entry_set_text(GTK_ENTRY(number_entry_p), number.c_str());
  
  if (std::strlen(gtk_entry_get_text(GTK_ENTRY(file_entry_p)))) {
    // reset this window as sensitive to make gtk_widget_grab_focus() work
    gtk_widget_set_sensitive(GTK_WIDGET(get_win()), true);
    gtk_widget_grab_focus(send_button_p);
  }
}

void MainWindow::about_impl(bool efax_gtk) {
  InfoDialog* dialog_p;

  if (efax_gtk) {
    std::string message("efax-gtk-" VERSION "\n");
    message += gettext("Copyright (C) 2001 - 2007 Chris Vine\n\n"
		       "This program is released under the "
		       "GNU General Public License, version 2");
    dialog_p = new InfoDialog(message.c_str(), gettext("About efax-gtk"),
			      GTK_MESSAGE_INFO, get_win());
  }
  else {
    std::string message = gettext("This program is a front end for efax.\n"
				  "efax is a program released under the "
				  "GNU General Public License, version 2 by Ed Casas.\n\n"
				  "The copyright to efax is held by Ed Casas "
				  "(the copyright to this program is held by Chris Vine)");
    dialog_p = new InfoDialog(message.c_str(), gettext("About efax"),
			      GTK_MESSAGE_INFO, get_win());
  }

  // there is no memory leak -- exec() was not called so InfoDialog
  // will delete its own memory when it is closed
}

void MainWindow::translations_impl(void) {

  std::string message = gettext("Italian - Luca De Rugeriis\n");
  message += gettext("Polish - Pawel Suwinski\n");
  message += gettext("Bulgarian - Zdravko Nikolov\n");
  message += gettext("Russian - Pavel Vainerman\n");
  message += gettext("Hebrew - Assaf Gillat\n");
  message += gettext("Greek - Hellenic Linux Users Group\n");
  message += gettext("Albanian - Besnik Bleta\n");
  message += gettext("Hungarian - Gergely Szakats\n");
  message += gettext("Simplified Chinese - Kite Lau\n");
  message += gettext("German - Steffen Wagner\n");
  message += gettext("Swedish - Daniel Nylander\n");
  message += gettext("Catalan - Jordi Sayol Salomo\n");
  message += gettext("Traditional Chinese - Wei-Lun Chao\n");

  new InfoDialog(message.c_str(), gettext("efax-gtk: Translations"),
		 GTK_MESSAGE_INFO, get_win());

  // there is no memory leak -- InfoDialog is modeless and will delete its own memory
  // when it is closed
}

void MainWindow::get_longest_button_text(void) {
  std::vector<std::string> text_vec;
  //text_vec.push_back(gettext("Single file"));
  text_vec.push_back(gettext("Scan File"));
//  text_vec.push_back(gettext("Multiple files"));
  text_vec.push_back(gettext("Socket list"));
  text_vec.push_back(gettext("Tel number: "));
  text_vec.push_back(gettext("Send fax"));
  text_vec.push_back(gettext("Answer call"));
  text_vec.push_back(gettext("Take over call"));
  text_vec.push_back(gettext("Standby"));
  text_vec.push_back(gettext("Stop"));

  std::vector<std::string>::const_iterator temp_iter;
  std::vector<std::string>::const_iterator max_width_iter;
  int width;
  int height;
  int max_width;

  // create a button to work on - the GobjHandle object will claim ownership of the button
  GobjHandle<GtkWidget> button_h(gtk_button_new());

  for (temp_iter = text_vec.begin(), max_width_iter = text_vec.begin(),
	 width = 0, height = 0, max_width = 0;
       temp_iter != text_vec.end(); ++temp_iter) {

    GobjHandle<PangoLayout> pango_layout_h(gtk_widget_create_pango_layout(button_h,
									 temp_iter->c_str()));
    pango_layout_get_pixel_size(pango_layout_h, &width, &height);

    if (width > max_width) {
      max_width = width;
      max_width_iter = temp_iter;
    }
  }
  max_text = *max_width_iter;
}

void MainWindow::tray_icon_left_clicked_slot(void) {

  GtkWidget* mainwin_p = GTK_WIDGET(get_win());
  // test GTK_WIDGET_VISIBLE() as well as the 'obscured' variable - if we call
  // hide() on the window it does not count as a visibility notify event!
  if (GTK_WIDGET_VISIBLE(mainwin_p)
      && !obscured
      && !minimised
      && GTK_WIDGET_SENSITIVE(mainwin_p)) {
    gtk_widget_hide(mainwin_p);
  }
  else gtk_window_present(get_win());
}

void MainWindow::tray_icon_menu_slot(int item) {

  switch(item) {

  case TrayItem::list_received_faxes:
    fax_list_impl(FaxListEnum::received);
    break;
  case TrayItem::list_sent_faxes:
    fax_list_impl(FaxListEnum::sent);
    break;
  case TrayItem::receive_answer:
    receive_impl(EfaxController::receive_answer);
    break;
  /*case TrayItem::receive_takeover:
    receive_impl(EfaxController::receive_takeover);
    break;*/
  case TrayItem::receive_standby:
    receive_impl(EfaxController::receive_standby);
    break;
  case TrayItem::stop:
    efax_controller.stop();
    break;
  case TrayItem::quit:
    close_impl();
    break;
  default:
    break;
  }
}

void MainWindow::style_set_impl(void) {

  // the main task is to set the buttons in MainWindow
  int width = 0;
  int height = 0;
  // create a button to work on - the GobjHandle object will claim ownership of the button
  GobjHandle<GtkWidget> button_h(gtk_button_new());

  GtkStyle* style_p = gtk_widget_get_style(GTK_WIDGET(get_win()));
  gtk_widget_set_style(button_h, style_p);

  GobjHandle<PangoLayout> pango_layout_h(gtk_widget_create_pango_layout(button_h,
							 max_text.c_str()));
  pango_layout_get_pixel_size(pango_layout_h, &width, &height);

  // add padding
  width += 18;
  height += 12;

  // have some sensible minimum width and height if a very small font chosen
  const int min_width = standard_size * 4;
  const int min_height = 30;
  if (width < min_width) width = min_width;
  if (height < min_height) height = min_height;

  gtk_widget_set_size_request(single_file_button_p, width, height);
  //gtk_widget_set_size_request(multiple_file_button_p, width, height);
  gtk_widget_set_size_request(socket_list_button_p, 0, 0);
  gtk_widget_set_size_request(number_button_p, width, height);
  gtk_widget_set_size_request(send_button_p, width, height);
  gtk_widget_set_size_request(receive_answer_button_p, width, height);
  //gtk_widget_set_size_request(receive_takeover_button_p, width, height);
  gtk_widget_set_size_request(receive_standby_button_p, width, height);
  gtk_widget_set_size_request(stop_button_p, width, height);

  // now set the status line style
  status_line.set_status_line(height - 6);
}

bool MainWindow::draw_fax_from_socket_notifier(void) {

/* This method displays the "red circle" indicator which shows a fax
   file received from the socket is awaiting sending.  It is is called
   in the following circumstances:

   - it is called in MainWindowCB::mainwin_drawing_area_expose_event(),
     handling the signal_expose_event signal of the
     MainWindow::drawing_area_p object

   - MainWindowCB::mainwin_drawing_area_expose_event() will in turn be
     called either because of a normal expose event on the desktop, or
     because MainWindow::socket_filelist_changed_notify_slot() has
     been invoked which calls gtk_widget_queue_draw() on the
     drawing_area_p object to force it to redraw
     (MainWindow::socket_filelist_changed_notify_slot() also calls
     SocketListDialog::set_socket_list_rows() via the
     MainWindow::update_socket_list signal so updating the socket list
     dialog if that dialog happens to be displayed)

   - MainWindow::socket_filelist_changed_notify_slot() is connected to
     the SocketServer::filelist_changed_notify Notifier object

   - the SocketServer::filelist_changed_notify signal is emitted by
     SocketServer:read_socket() when a new fax file to be sent is
     received via the socket and also by SocketServer::remove_file().

   - SocketServer::remove_file() is called by
     MainWindow::remove_from_socket_server_filelist()

   - MainWindow::remove_from_socket_server_filelist() is connected to
     the EfaxController::remove_from_socket_server_filelist signal and
     to the SocketListDialog::remove_from_socket_server_filelist
     signal

   - the EfaxController::remove_from_socket_server_filelist signal is
     emitted by EfaxController::timer_event() when a fax derived from
     the socket list is successfully sent

   - the SocketListDialog::remove_from_socket_server_filelist signal is
     emitted by SocketListDialog::remove_file()

   - SocketListDialog::remove_file() is connected to the
     PromptDialog::accepted signal (emitted when a user elects to
     delete a queued fax from the list of files received for faxing
     from the socket)
*/

  if (GTK_WIDGET_REALIZED(drawing_area_p)) { // if we started hidden in the system tray
                                             // the drawing area may not yet be realised

    GdkDrawable* drawable_p = GDK_DRAWABLE(drawing_area_p->window);

    if (!socket_server.get_filenames().first->empty()) {

      if (!red_gc_h.get()) {
      
	GdkColor red;
	red.red = static_cast<guint16>(0.7 * 65535.0);
	red.green = 0;
	red.blue = 0;

	GdkColormap* colormap_p = gtk_widget_get_default_colormap();
	if (!gdk_colormap_alloc_color(colormap_p, &red, false, true)) {
	  write_error("Error allocating colour in colourmap in "
		      "MainWindow::draw_fax_from_socket_notifier()\n");
	}
	else {
	  red_gc_h = GobjHandle<GdkGC>(gdk_gc_new(drawable_p));
	  gdk_gc_set_foreground(red_gc_h, &red);
	}
      }

      if (red_gc_h.get()) {

	GdkGC* white_gc_p = gtk_widget_get_style(drawing_area_p)->white_gc;

	gdk_draw_arc(drawable_p, white_gc_p,
		     true, 6, 6,
		     standard_size - 12, standard_size - 12,
		     0, 64 * 360);
	
	gdk_draw_arc(drawable_p, red_gc_h,
		     true, 8, 8,
		     standard_size - 16, standard_size - 16,
		     0, 64 * 360);
      }
    }

    else {
      
      GdkGC* background_gc_p =
	gtk_widget_get_style(drawing_area_p)->bg_gc[GTK_WIDGET_STATE(drawing_area_p)];
      gdk_draw_rectangle(drawable_p, background_gc_p,
			 true, 0, 0,
			 standard_size, standard_size);
    } 
  }
  // processing stops here
  return true;
}

void MainWindow::strip(std::string& text) {

  // erase any trailing space or tab
  while (!text.empty() && text.find_last_of(" \t") == text.size() - 1) {
    text.resize(text.size() - 1);
  }
  // erase any leading space or tab
  while (!text.empty() && (text[0] == ' ' || text[0] == '\t')) {
    text.erase(0, 1);
  }
}

void MainWindow::start_pipe_thread(void) {

  // needed for pipe_thread() - we need to get the program PID here, as
  // under the linuxthreads implementation of pthreads, getpid() will
  // yield a different value for different threads in the same process
  // and we need to use the PID yielded by the main (GUI) thread
  prog_pid = getpid();
  // read ProgConfig::working_dir in this thread - if we read it in
  // MainWindow::pipe_thread() we would have to protect accesses with
  // the ProgConfig::mutex_p global mutex since not all implementations
  // of std::string provide concurrent read access, and it is best to
  // avoid use of the global mutex where that can easily be done
  prog_fifo_name = prog_config.working_dir;
  prog_fifo_name += PIPE_NAME;

  // block off the signals for which we have set handlers so that the pipe
  // thread does not receive the signals, otherwise we will have memory synchronisation
  // issues in multi-processor systems - we will unblock in the initial (GUI) thread
  // as soon as the pipe thread has been launched
  sigset_t sig_mask;
  sigemptyset(&sig_mask);
  sigaddset(&sig_mask, SIGCHLD);
  sigaddset(&sig_mask, SIGQUIT);
  sigaddset(&sig_mask, SIGTERM);
  sigaddset(&sig_mask, SIGINT);
  sigaddset(&sig_mask, SIGHUP);
  pthread_sigmask(SIG_BLOCK, &sig_mask, 0);

  std::auto_ptr<Thread::Thread> res =
    Thread::Thread::start(Callback::make(*this, &MainWindow::pipe_thread),
			  false);
  if (!res.get()) {
    write_error("Cannot start new pipe thread\n");
  }
  // now unblock the signals so that the initial (GUI) thread can receive them
  pthread_sigmask(SIG_UNBLOCK, &sig_mask, 0);
}

void MainWindow::pipe_thread(void) {

  if(access(prog_fifo_name.c_str(), F_OK) == -1) {
    // fifo doesn't exit - create it
    if (mkfifo(prog_fifo_name.c_str(), 0666) != 0) {
      write_error("Error creating fifo in MainWindow::pipe_thread()\n");
      return;
    }
  }

  for (;;) {
    int read_fd = open(prog_fifo_name.c_str(), O_RDONLY);
    if(read_fd == -1) {
      write_error("Error opening fifo in MainWindow::pipe_thread()\n");
      return;
    }
    Attachable::In pipe_stream;

#ifdef HAVE_STREAM_IMBUE
    pipe_stream.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

    pipe_stream.attach(read_fd);
    
    pid_t pid = 0;
    pipe_stream >> pid;

    // we do not need a mutex for prog_pid as we only use
    // it in this thread once this thread has been launched
    if (pid == prog_pid) present_window_notify();
    
    pipe_stream.close();
  }
}

void MainWindow::present_window_slot(void) {
  gtk_widget_hide(GTK_WIDGET(get_win())); // so window will come up in another
                                          // workspace if necessary
  gtk_window_present(get_win());
}

bool MainWindow::read_error_pipe_slot(void) {
  char pipe_buffer[PIPE_BUF + 1];
  ssize_t result;

  while ((result = MainWindow::error_pipe.read(pipe_buffer, PIPE_BUF)) > 0) {
    SharedHandle<char*> output_h(error_pipe_reassembler(pipe_buffer, result));
    if (output_h.get()) text_window.write_red_slot(output_h.get());
    else write_error(gettext("Invalid Utf8 received in MainWindow::read_error_pipe_slot()\n"));
  }
  return true; // retain connection for further reads
}


MessageText::MessageText(GtkWindow* win_p):
                            MainWidgetBase(gtk_scrolled_window_new(0, 0)),
                            window_p(win_p) {

  text_view_p = gtk_text_view_new();
  gtk_container_add(GTK_CONTAINER(get_main_widget()), text_view_p);

  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view_p), GTK_WRAP_WORD);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_p), false);
  GTK_WIDGET_UNSET_FLAGS(text_view_p, GTK_CAN_FOCUS);

  GtkScrolledWindow* scrolled_window_p = GTK_SCROLLED_WINDOW(get_main_widget());
  gtk_scrolled_window_set_shadow_type(scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(scrolled_window_p, GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_ALWAYS);

  GdkColor red;
  red.red = static_cast<guint16>(0.8 * 65535.0);
  red.green = 0;
  red.blue = 0;
  // GtkTextTags are not owned by a GTK+ container when passed to it so use a GobjHandle
  red_tag_h = GobjHandle<GtkTextTag>(gtk_text_tag_new(0));

  GdkColormap* colormap_p = gtk_widget_get_default_colormap();
  if (!gdk_colormap_alloc_color(colormap_p, &red, false, true)) {
    write_error("Error allocating colour in colourmap in "
		"MessageText::MessageText()\n");
  }
  else {
    g_object_set(red_tag_h.get(),
		 "foreground-gdk", &red,
		 static_cast<void*>(0));
  }

  GtkTextBuffer* buffer_p = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_p));
  gtk_text_tag_table_add(gtk_text_buffer_get_tag_table(buffer_p), red_tag_h);

  // keep an end mark to assist scrolling to the end of buffer
  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(buffer_p, &end_iter);
  // despite the name, the caller of gtk_text_buffer_create_mark() does not own
  // a reference count to the mark - the mark is owned by the text buffer, so
  // we do not need to put it in a GobjHandle handle
  end_mark_p = gtk_text_buffer_create_mark(buffer_p, 0, &end_iter, false);

  // save the current working directory before MainWindow::MainWindow can get
  // at it, in case it is needed in future by reset_logfile

#ifndef PATH_MAX
#define PATH_MAX 1023
#endif
  int count;
  void* success;
  std::string default_dirname;
  for (count = 1, success = 0; !success && count < 10; count++) {
    ScopedHandle<char*> path_buffer_h(new char[(PATH_MAX * count) + 1]);
    success = getcwd(path_buffer_h.get(), PATH_MAX * count);
    if (success) default_dirname = path_buffer_h.get();
  }
  // local named object temp_a required in order to compile with gcc-2.95.3
  std::auto_ptr<Logger> temp_a(new Logger(default_dirname.c_str(), window_p));
  logger_a = temp_a;
}

void MessageText::cleanify(std::string& message) {

  std::string::size_type chunk_start = 0;
  std::string::size_type chunk_end;
  while (chunk_start != std::string::npos) {
    std::string::size_type chunk_size;
    chunk_end = message.find('\n', chunk_start);

    // include in the search chunk any '\n' found
    if (chunk_end + 1 >= message.size()) chunk_end = std::string::npos;
    else if (chunk_end != std::string::npos) chunk_end++;

    // now search the relevant string/substring and erase from message
    if (chunk_end == std::string::npos) chunk_size = message.size() - chunk_start;
    else chunk_size = chunk_end - chunk_start;
    if (message.substr(chunk_start, chunk_size).find(" Copyright ") != std::string::npos
	|| message.substr(chunk_start, chunk_size).find(" compiled ") != std::string::npos
	|| message.substr(chunk_start, chunk_size).find("terminating on signal 15") != std::string::npos) {
      message.erase(chunk_start, chunk_size);
      // if we have erased this chunk then we don't want to reset chunk_start
      // unless we have finished examining message
      if (chunk_end == std::string::npos) chunk_start = std::string::npos;
    }
    // if we didn't erase the last chunk, then we need to reset
    // chunk_start to beginning of next substring (or to string::npos)
    else chunk_start = chunk_end;
  }
}

void MessageText::write_black_slot(const char* message) {
  std::string temp(message);
  cleanify(temp);

  bool scrolling = false;
  GtkAdjustment* adj_p = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(get_main_widget()));
  if (adj_p->value >= (adj_p->upper - adj_p->page_size - 1e-12)) {
    scrolling = true;
  }
  
  GtkTextBuffer* buffer_p = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_p));
  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(buffer_p, &end_iter);
  gtk_text_buffer_insert(buffer_p, &end_iter, temp.data(), temp.size());

  if (scrolling) {
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(text_view_p), end_mark_p,
				 0.0, false, 0.0, 0.0);
  }

  // now relay to standard output
  ssize_t result;
  ssize_t written = 0;
  ssize_t to_write_count = temp.size();
  const char* write_text = temp.c_str();
  do {
    result = ::write(1, write_text + written, to_write_count);
    if (result > 0) {
      written += result;
      to_write_count -= result;
    }
  } while (to_write_count && (result != -1 || errno == EINTR));

  // now log to file if required
  logger_a->write_to_log(write_text);
}

void MessageText::write_red_slot(const char* message) {
  std::string temp(message);
  cleanify(temp);

  bool scrolling = false;
  GtkAdjustment* adj_p = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(get_main_widget()));
  if (adj_p->value >= (adj_p->upper - adj_p->page_size - 1e-12)) {
    scrolling = true;
  }
  
  GtkTextBuffer* buffer_p = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_p));
  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(buffer_p, &end_iter);
  gtk_text_buffer_insert_with_tags(buffer_p, &end_iter, temp.data(), temp.size(),
				   red_tag_h, static_cast<void*>(0));

  if (scrolling) {
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(text_view_p), end_mark_p,
				 0.0, false, 0.0, 0.0);
  }

  // now relay to standard error
  ssize_t result;
  ssize_t written = 0;
  ssize_t to_write_count = temp.size();
  const char* write_text = temp.c_str();
  do {
    result = ::write(2, write_text + written, to_write_count);
    if (result > 0) {
      written += result;
      to_write_count -= result;
    }
  } while (to_write_count && (result != -1 || errno == EINTR));

  // now log to file if required
  logger_a->write_to_log(write_text);
}

StatusLine::StatusLine(const int size):
                                 MainWidgetBase(gtk_hbox_new(false, 0)),
				 standard_size(size) {

  status_label_p = gtk_label_new(gettext("Inactive"));
  GtkWidget* help_label_p = gtk_label_new(gettext("Press F1 for help"));

  GtkWidget* status_frame_p = gtk_frame_new(0);
  GtkWidget* help_frame_p = gtk_frame_new(0);
  gtk_frame_set_shadow_type(GTK_FRAME(status_frame_p), GTK_SHADOW_IN);
  gtk_frame_set_shadow_type(GTK_FRAME(help_frame_p), GTK_SHADOW_IN);

  gtk_container_add(GTK_CONTAINER(status_frame_p), status_label_p);
  gtk_container_add(GTK_CONTAINER(help_frame_p), help_label_p);
  
  gtk_box_pack_start(GTK_BOX(get_main_widget()), status_frame_p, true, true, 0);
  gtk_box_pack_end(GTK_BOX(get_main_widget()), help_frame_p, true, true, 0);

  gtk_widget_set_size_request(status_label_p, standard_size * 7, -1);
  gtk_widget_set_size_request(help_label_p, standard_size * 6, -1);
  
  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), 2);
  gtk_widget_show_all(get_main_widget());
}

void StatusLine::set_status_line(int new_height) {

  // set the height
  if (new_height < standard_size + 2) new_height = standard_size + 2;
  gtk_widget_set_size_request(get_main_widget(), -1, new_height);

  // make the status label display in red
  GdkColor red;
  red.red = static_cast<guint16>(0.9 * 65535.0);
  red.green = 0;
  red.blue = 0;

  // we don't need to allocate the GdkColor red object before it is
  // used by gtk_widget_modify_fg() - that function will do it itself
  gtk_widget_modify_fg(status_label_p, GTK_STATE_NORMAL, &red);
}

void StatusLine::write_status_slot(const char* text) {
  gtk_label_set_text(GTK_LABEL(status_label_p), text);
}


int connect_to_stderr(void) {
  int result = MainWindow::error_pipe.connect_to_stderr();
  if (!result) MainWindow::connected_to_stderr = true;
  return result;
} 

ssize_t write_error(const char* message) {
  // this writes to a pipe (which will provide asynchronous to
  // synchronous conversion) so it can be used by different threads
  // without upsetting GTK+. It also only uses async-signal-safe
  // system functions if connect_to_stderr() has been called (which
  // is only in a child process), and in the main process (where
  // connect_to_stderr() is not called) is thread safe with only a
  // mutex in this function because (a) although PipeFifo::read()
  // and PipeFifo::write() check the value of PipeFifo::read_fd and
  // PipeFifo::write_fd respectively, and PipeFifo::open(),
  // PipeFifo::make_writeonly(), PipeFifo::make_readonly(),
  // PipeFifo::close(), PipeFifo::connect_to_stdin(),
  // PipeFifo::connect_to_stdout() and PipeFifo::connect_to_stderr()
  // change those values, all but the last one are never called on
  // MainWindow::error_pipe until (if at all) error_pipe goes out of
  // scope when global objects are destroyed as the program exits, and
  // (b) PipeFifo::connect_to_stderr() is only ever called after
  // fork()ing into a new single-threaded process

  // The pipe is non-blocking - it will be emptied fairly promptly
  // but messages should be less than PIPE_BUF in length or any excess
  // will be lost unless the caller checks the return value and acts
  // accordingly

  ssize_t result = 0;
  ssize_t written = 0;
  ssize_t to_write_count = std::strlen(message);
  
  // if write_error_mutex_p is NULL then this must be the static
  // pipe being written to before the MainWindow constructor is
  // entered - we might was well check this condition in this
  // function although it should not happen
  if (!MainWindow::connected_to_stderr && write_error_mutex_p) {
    Thread::Mutex::Lock lock(*write_error_mutex_p);
    do {
      result = MainWindow::error_pipe.write(message + written, to_write_count);
      if (result > 0) {
	written += result;
	to_write_count -= result;
      }
    } while (to_write_count && result != -1); // the pipe checks for EINTR itself
  }

  else {
    // no mutex is required in this case: connect_to_stderr() is only
    // called after fork()ing and it is for the new process to synchronise
    // its writes to standard error (in efax-gtk child processes are all
    // single-threaded anyway) and as between different processes, POSIX
    // guarantees that writes not larger than PIPE_BUF in size will not be
    // interleaved.  Since no mutex is used in this case, this function is
    // async-signal-safe for child processes.  It can therefore be used
    // after fork()ing before exec()ing even though this is a multi-threaded
    // program
    do {
      result = ::write(2, message + written, to_write_count);
      if (result > 0) {
	written += result;
	to_write_count -= result;
      }
    } while (to_write_count && (result != -1 || errno == EINTR));
  }
  return written;
}

void close_signalhandler(int) {
  close_flag = true;
}
