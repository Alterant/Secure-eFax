/* Copyright (C) 2004 and 2006 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkbbox.h>
#include <gtk/gtkhbbox.h>

#include "addressbook.h"
#include "socket_notify.h"

#ifdef HAVE_OSTREAM
#include <ios>
#include <ostream>
#endif

#ifdef HAVE_STRINGSTREAM
#include <sstream>
#else
#include <strstream>
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

namespace { // callback for internal use only

void SocketNotifyDialogCB::socket_notify_button_clicked(GtkWidget* widget_p, void* data) {
  SocketNotifyDialog* instance_p = static_cast<SocketNotifyDialog*>(data);

  if (widget_p == instance_p->number_button_p) {
    if (!AddressBook::get_is_address_list()) {
      AddressBook* dialog_p = new AddressBook(instance_p->standard_size,
					      instance_p->get_win());
      dialog_p->accepted.connect(sigc::mem_fun(*instance_p,
					       &SocketNotifyDialog::set_number_slot));
      // there is no memory leak -- AddressBook will delete its own memory
      // when it is closed
    }
  }
  else if (widget_p == instance_p->send_button_p) {
    gtk_widget_hide_all(GTK_WIDGET(instance_p->get_win()));
    instance_p->send_signals();
    instance_p->close();
  }
  else if (widget_p == instance_p->queue_button_p) {
    instance_p->close();
  }
  else {
    write_error("Callback error in SocketNotifyDialogCB::socket_notify_button_clicked()\n");
    instance_p->close();
  }
}

} // anonymous namespace


SocketNotifyDialog::SocketNotifyDialog(const int size, 
				       const std::pair<std::string, unsigned int>& fax_pair):
                                               WinBase(gettext("efax-gtk: print job received on socket"),
						       prog_config.window_icon_h),
					       standard_size(size) {
  
  number_button_p = gtk_button_new_with_label(gettext("Tel number: "));
  send_button_p = gtk_button_new_with_label(gettext("Send fax"));
  queue_button_p = gtk_button_new_with_label(gettext("Queue fax"));
    
  number_entry_p = gtk_entry_new();
  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* number_box_p = gtk_hbox_new(false, 0);
  GtkWidget* window_box_p = gtk_vbox_new(false, 0);

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(button_box_p), standard_size/3);
  gtk_container_add(GTK_CONTAINER(button_box_p), queue_button_p);
  gtk_container_add(GTK_CONTAINER(button_box_p), send_button_p);

  fax_name.second = fax_pair.first;
#ifdef HAVE_STRINGSTREAM
  std::ostringstream strm;
  strm << gettext("PRINT JOB: ") << fax_pair.second;

  fax_name.first = strm.str();
  strm << gettext(" has been received on socket.\n"
		  "Send or queue fax?");
  GtkWidget* label_p = gtk_label_new(strm.str().c_str());
#else
  std::ostrstream strm1;
  std::ostrstream strm2;

  strm1 << gettext("PRINT JOB: ") << fax_pair.second << std::ends;
  const char* text_p = strm1.str();
  fax_name.first = text_p;
  delete[] text_p;

  strm2 << gettext("PRINT JOB: ") << fax_pair.second
	<< gettext(" has been received on socket.\n"
		   "Send or queue fax?")
	<< std::ends;

  text_p = strm2.str();
  GtkWidget* label_p = gtk_label_new(text_p);
  delete[] text_p;
#endif

  gtk_widget_set_size_request(number_entry_p, standard_size * 7, standard_size);
  
  gtk_box_pack_start(GTK_BOX(number_box_p), number_button_p, false, false, standard_size/3);
  gtk_box_pack_start(GTK_BOX(number_box_p), number_entry_p, true, true, standard_size/3);

  gtk_box_pack_start(GTK_BOX(window_box_p), label_p, true, true, standard_size/2);
  gtk_box_pack_start(GTK_BOX(window_box_p), number_box_p, false, false, standard_size/3);
  gtk_box_pack_start(GTK_BOX(window_box_p), button_box_p, false, false, standard_size/3);

  g_signal_connect(G_OBJECT(number_button_p), "clicked",
		   G_CALLBACK(SocketNotifyDialogCB::socket_notify_button_clicked), this);
  g_signal_connect(G_OBJECT(send_button_p), "clicked",
		   G_CALLBACK(SocketNotifyDialogCB::socket_notify_button_clicked), this);
  g_signal_connect(G_OBJECT(queue_button_p), "clicked",
		   G_CALLBACK(SocketNotifyDialogCB::socket_notify_button_clicked), this);

  GTK_WIDGET_SET_FLAGS(number_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(send_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(queue_button_p, GTK_CAN_DEFAULT);

  gtk_container_add(GTK_CONTAINER(get_win()), window_box_p);
  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/2);
  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_widget_grab_focus(number_button_p);
  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

void SocketNotifyDialog::set_number_slot(const std::string& number) {

  if (!number.empty()) {
    gtk_entry_set_text(GTK_ENTRY(number_entry_p), number.c_str());
    // reset this window as sensitive to make grab_focus() work
    gtk_widget_set_sensitive(GTK_WIDGET(get_win()), true);
    gtk_widget_grab_focus(queue_button_p);
  }
}

void SocketNotifyDialog::send_signals(void) {

  fax_name_sig(fax_name);
  fax_number_sig(gtk_entry_get_text(GTK_ENTRY(number_entry_p)));
  sendfax_sig();
}
