/* Copyright (C) 2001 2002 and 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef SETTINGS_HELP_H
#define SETTINGS_HELP_H

#include "prog_defs.h"

#include <vector>
#include <string>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#include "utils/window.h"


struct SettingsMessagesBase {

  std::vector<std::string> captions;
  std::vector<std::string> messages;

  std::string get_caption(std::vector<std::string>::size_type);
  std::string get_message(std::vector<std::string>::size_type);

  SettingsMessagesBase(std::vector<std::string>::size_type size): captions(size), messages(size) {}
};

class IdentityMessages: private SettingsMessagesBase {
public:
  enum {name = 0, number};
  IdentityMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class ModemMessages: private SettingsMessagesBase {
public:
  enum {device = 0, lock, modem_class, dialmode, capabilities, rings};
  ModemMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class ParmsMessages: private SettingsMessagesBase {
public:
  enum {init = 0, reset, extra_parms};
  ParmsMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class PrintMessages: private SettingsMessagesBase {
public:
  enum {gtkprint = 0, command, shrink, popup};
  PrintMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class ViewMessages: private SettingsMessagesBase {
public:
  enum {ps_view_command = 0};
  ViewMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class SockMessages: private SettingsMessagesBase {
public:
  enum {run_server = 0, popup, port, client_address};
  SockMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class ReceiveMessages: private SettingsMessagesBase {
public:
  enum {popup = 0, exec};
  ReceiveMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class SendMessages: private SettingsMessagesBase {
public:
  enum {res = 0, header, dial_prefix};
  SendMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class LoggingMessages: private SettingsMessagesBase {
public:
  enum {logfile = 0};
  LoggingMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};

class PageMessages: private SettingsMessagesBase {
public:
  enum {page = 0};
  PageMessages(void);

  using SettingsMessagesBase::get_caption;
  using SettingsMessagesBase::get_message;
};


namespace { // we put the function in anonymous namespace in settings_help.cpp
            // so it is not exported at link time
namespace SettingsHelpDialogCB {
  extern "C"  void settings_help_button_clicked(GtkWidget*, void*);
}
}

class SettingsHelpDialog: public WinBase {

  GtkWidget* close_button_p;
public:
  friend void SettingsHelpDialogCB::settings_help_button_clicked(GtkWidget*, void*);

  SettingsHelpDialog(const int standard_size, const std::string& text,
		     const std::string& caption, GtkWindow* parent_p);
};

#endif
