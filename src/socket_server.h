/* Copyright (C) 2003 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include "prog_defs.h"

#include <string>
#include <list>
#include <utility>
#include <memory>

#include <netinet/in.h>

#include <sigc++/sigc++.h>

#include "utils/sem_sync.h"
#include "utils/shared_ptr.h"
#include "utils/thread.h"
#include "utils/mutex.h"
#include "utils/notifier.h"

typedef std::list<std::pair<std::string, unsigned int> > FilenamesList;


class SocketServer: public sigc::trackable {

  bool server_running;
  bool other_sock_client_address;
  int serve_sock_fd;
  unsigned int count;
  std::string working_dir;
  Thread::Mutex working_dir_mutex;

  // make filenames_mutex mutable so that we can use it in
  // the const member function SocketServer::get_filenames()
  // we can use a SharedPtr rather than a SharedLockPtr
  // object here - filenames_mutex will do the locking of the
  // contained object and we do not actually copy filenames_s
  // in more than one thread (it is only copied in the main (GUI)
  // thread)
  SharedPtr<FilenamesList> filenames_s;
  mutable Thread::Mutex filenames_mutex;

  std::auto_ptr<Thread::Thread> socket_thread_h;

  Thread::Mutex stdout_mutex;
  Thread::Mutex fax_to_send_mutex;

  Notifier socket_error_notify;

  Notifier stdout_notify;
  Thread::Cond stdout_cond;
  Thread::Cond fax_to_send_cond;

  std::string stdout_text;
  void write_stdout_from_thread(const char*);
  void write_stdout_dispatcher_slot(void);

  std::string port;

  std::pair<std::string, unsigned int> fax_to_send;

  void socket_thread(void);
  bool accept_on_client(void);
  void read_socket(int, int, const char*);
  void read_queued_faxes(void);
  void save_queued_faxes(void);
  bool is_valid_peer(const sockaddr_in&);
  bool addr_compare(const std::string&, const std::string&);
  void cleanup(void);

  // this is a concrete class not to be copied or assigned
  SocketServer(const SocketServer&);
  void operator=(const SocketServer&);
public:

  // Notifier is a thread safe signal
  // the main GUI thread should connect to it to be
  // notified when the server has received a fax which
  // is to be sent by efax-gtk or a fax has been
  // removed from the list using SocketServer::remove_file()
  Notifier filelist_changed_notify;
  // this signal will also be emitted if the reason for the
  // change was a fax received from the socket for sending
  // (the fax particulars can be obtained with get_fax_to_send())
  Notifier fax_to_send_notify;

  sigc::signal1<void, const char*> stdout_message;

  void add_file(const std::string&);
  int remove_file(const std::string&);
  std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> > get_filenames(void) const;
  std::pair<std::string, unsigned int> get_fax_to_send(void);  
  bool is_server_running(void) const {return server_running;}
  std::string get_port(void) const {return port;}
  bool get_other_sock_client_address(void) const {return other_sock_client_address;}
  int get_count(void) const {return count;}
  void start(const std::string& port, bool other_sock_client_address);
  void stop(void);
  SocketServer(void);
  ~SocketServer(void) {if (server_running) stop();}
};

#endif
