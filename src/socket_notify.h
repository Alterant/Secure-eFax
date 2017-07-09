/* Copyright (C) 2004 and 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef SOCKET_NOTIFY_H
#define SOCKET_NOTIFY_H

#include "prog_defs.h"

#include <string>
#include <utility>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#include <sigc++/sigc++.h>

#include "utils/window.h"


namespace { // we put the function in anonymous namespace in socket_notify.cpp
            // so it is not exported at link time
namespace SocketNotifyDialogCB {
  extern "C"  void socket_notify_button_clicked(GtkWidget*, void*);
}
}

class SocketNotifyDialog: public sigc::trackable, public WinBase {
  const int standard_size;
  std::pair<std::string, std::string> fax_name;

  GtkWidget* number_button_p;
  GtkWidget* send_button_p;
  GtkWidget* queue_button_p;

  GtkWidget* number_entry_p;

  void set_number_slot(const std::string&);
  void send_signals(void);
public:
  friend void SocketNotifyDialogCB::socket_notify_button_clicked(GtkWidget*, void*);

  sigc::signal1<void, const std::pair<std::string, std::string>&> fax_name_sig;
  sigc::signal1<void, const std::string&> fax_number_sig;
  sigc::signal0<void> sendfax_sig;

  SocketNotifyDialog(const int standard_size, const std::pair<std::string, unsigned int>&);
};

#endif
