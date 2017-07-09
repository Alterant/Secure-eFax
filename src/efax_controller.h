/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef EFAX_CONTROLLER_H
#define EFAX_CONTROLLER_H

#include "prog_defs.h"

#include <string>
#include <vector>
#include <utility>

#include <unistd.h>
#include <sys/types.h>

#include <sigc++/sigc++.h>

#include "utils/pipes.h"
#include "utils/sem_sync.h"
#include "utils/notifier.h"
#include "utils/io_watch.h"
#include "utils/utf8_utils.h"


struct Fax_item {
  std::vector<std::string> file_list;
  std::string number;
  bool is_socket_file;
};

class EfaxController: public sigc::trackable {

public:
  enum State {inactive = 0, sending, receive_answer, receive_takeover, receive_standby, start_send_on_standby, send_on_standby};

private:
  State state;
  bool close_down;

  int received_fax_count;

  SemSync fax_made_sem;
  Notifier fax_made_notify;
  Notifier no_fax_made_notify;

  std::vector<std::string> sendfax_parms_vec;
  Fax_item last_fax_item_sent;

  PipeFifo stdout_pipe;
  guint iowatch_tag;
  Utf8::Reassembler stdout_pipe_reassembler;

  void receive_cleanup(void);
  std::pair<std::string, std::string> save_received_fax(void);
  std::pair<std::string, std::string> save_sent_fax(void);
  void join_child(void);
  void unjoin_child(void);
  void kill_child(void);
  void cleanup_fax_send_fail(void);
  std::vector<std::string> state_messages;
  void make_fax_thread(void);
  void init_sendfax_parms(void);
  std::pair<const char*, char* const*> get_sendfax_parms(void);
  void sendfax_slot(void);
  void no_fax_made_slot(void);
  bool read_pipe_slot(void);
  void sendfax_impl(const Fax_item&, bool);

  std::pair<const char*, char* const*> get_gs_parms(const std::string&);
  std::pair<const char*, char* const*> get_receive_parms(int);
  void delete_parms(std::pair<const char*, char* const*>);

  // we don't want to permit copies of this class
  EfaxController(const EfaxController&);
  void operator=(const EfaxController&);

public:
  sigc::signal1<void, const std::pair<std::string, std::string>&> fax_received_notify;
  sigc::signal1<void, const std::pair<std::string, std::string>&> fax_sent_notify;
  sigc::signal0<void> ready_to_quit_notify;
  sigc::signal1<void, const char*> stdout_message;
  sigc::signal1<void, const char*> write_state;
  sigc::signal1<void, const std::string&> remove_from_socket_server_filelist;

  void timer_event(void);
  void display_state(void) {write_state(state_messages[state].c_str());}
  int get_state(void) const {return state;}
  bool is_receiving_fax(void) const;

  int get_count(void) {return received_fax_count;}
  void reset_count(void) {received_fax_count = 0; display_state();}

  void stop(void);

  void efax_closedown(void);
  void sendfax(const Fax_item& fax_item) {sendfax_impl(fax_item, false);}
  void receive(State);
  EfaxController(void);
};

#endif
