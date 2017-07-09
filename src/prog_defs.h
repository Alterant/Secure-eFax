/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef PROG_DEFS_H
#define PROG_DEFS_H

#include <unistd.h>
#include <config.h>

// deal with any configuration issues arising from config.h

#ifndef ENABLE_NLS
inline const char* gettext(const char* text) {
  return text;
}
#endif

#ifndef HAVE_SSIZE_T
typedef signed int ssize_t;
#endif

#ifndef HAVE_SOCKLEN_T
typedef unsigned int socklen_t;
#endif

#ifndef HAVE_IN_ADDR_T
typedef unsigned long in_addr_t;
#endif

// deal with libsigc++
#if SIGC_VERSION < 20
#include "sigc_compatibility.h"
#endif

// define some common exit codes

#define MEM_ERROR 10
#define CONFIG_ERROR 11
#define COM_ERROR 12
#define FILEOPEN_ERROR 13
#define PIPE_ERROR 14
#define TERM_ERROR 15
#define ARG_ERROR 16
#define EXEC_ERROR 17
#define FORK_ERROR 18

#define RC_FILE "efax-gtkrc"

#include <sys/types.h>
#include <vector>
#include <string>
#include <gdk/gdkpixbuf.h>
#include "utils/gobj_handle.h"

// forward declaration for Prog_config
namespace Thread {
  class Mutex;
}


using namespace std;    // until I get a Std C++ compliant compiler to test with

#define PIPE_NAME "/.efax-gtk-pipe"

/************************* threading issues *************************

1. This program uses certain functions which are not guaranteed by
   IEEE Std 1003.1 to be thread safe, and which are not protected by
   mutexes because this program only uses them in one thread.  These
   functions are:

     localtime() - used in the main (GUI) thread (efax_controller.cpp)
     readdir()   - used in the main (GUI) thread (fax_list.cpp)

     gethostbyaddr() )
     gethostbyname() ) used in the socket server thread (socket_server.cpp)
     inet_ntoa()     )

   If the program is modified to use any of these in a different thread,
   mutexes will need to be applied.

2. The program operates the following threads:

   - the initial GUI thread (the main program thread) (runs throughout
     program execution).

   - a thread to read the fifo which is used to tell the program (when
     it has been started) that the user has tried to start it again,
     so that it can present itself instead of starting up again.  This
     involves the following method of class MainWindow:

       pipe_thread()

   - if the socket server is running, a socket server thread (runs
     whenever the socket server has been selected to run).  This
     involves the following methods of the SocketServer class:
       
       socket_thread(), accept_on_client(), is_valid_peer(),
       read_socket(), add_file(), write_stdout_from_thread() and
       save_queued_faxes().  (save_queued_faxes is called by both the
       main GUI thread and by the socket_server_thread - see the
       comments in that method about mutexes)

   - a temporary thread created to convert a postscript file to tiffg3
     files for faxing, involving the following methods of the
     EfaxController class:

       make_fax_thread() and get_gs_parms()

   - a temporary thread to prepare a fax for printing, involving the
     following methods of the FaxListDialog class:

       print_fax_thread(), write_pipe_to_file(),
       get_print_from_stdin_parms(), get_fax_to_ps_parms() and
       delete_parms() (write_pipe_to_file() and get_fax_to_ps_parms()
       is also called by the temporary thread invoked with
       view_fax_thread() and delete_parms() is also called by that
       thread and by the main (GUI) thread)

     together with FilePrintManager::set_filename() and
     FilePrintManager::print() if the program is compiled against
     GTK+-2.10 or higher.

   - a temporary thread to view a fax, involving the following methods
     of the FaxListDialog class:

       view_fax_thread(), write_pipe_to_file(), get_ps_viewer_parms(),
       get_fax_to_ps_parms() and delete_parms() (write_pipe_to_file()
       and get_fax_to_ps_parms() is also called by the temporary
       thread invoked with print_fax_thread() and delete_parms() is
       also called by that thread and by the main (GUI) thread)

3. The last member of the Prog_config class below is a pointer to a mutex object
   allocated in int main() in main.cpp.  The mutex should be locked whenever:

   (a) the main (GUI) thread is modifying a member of the Prog_config class, or
   (b) any worker thread is reading the Prog_config object.

   No worker thread should modify a Prog_config class member, to save excessive
   locking in this program

4. In addition, members of the Prog_config class which are not built-in types,
   and for which concurrent reads in different threads are not guaranteed, are
   read outside the main (GUI) thread as follows -

     - Prog_config::resolution is read by EfaxController::get_gs_parms()
       (in efax_controller.cpp)

     - Prog_config::page_size is read by EfaxController::get_gs_parms()
       (in efax_controller.cpp) and by FaxListDialog::get_fax_to_ps_parms() (in
       fax_list.cpp)

     - Prog_config::page_dim is read by FaxListDialog::get_fax_to_ps_parms() (in
       fax_list.cpp)

     - Prog_config::print_shrink is read by FaxListDialog::get_fax_to_ps_parms()
       (in fax_list.cpp)

     - Prog_config::print_cmd is read by FaxListDialog::get_print_from_stdin_parms()
       (in fax_list.cpp)

     - Prog_config::ps_view_cmd is read by FaxListDialog::get_ps_viewer_parms()
       (in fax_list.cpp)

     - Prog_config::permitted_clients_list is read by SocketServer::is_valid_peer()
       (in socket_server.cpp)

   These are protected by the mutex where read by these non-GUI
   threads and written to by the main (GUI) thread, but they should
   also be protected by the mutex wherever read by the main (GUI)
   thread unless it is known that there can be no concurrency with the
   execution of these other threads so far as concerns the reading of
   these members.  (In fact, as this program is written concurrent
   reads are only an issue with Prog_config::ps_view_cmd, and locking
   of it takes place in FileReadSelectDialog::get_view_file_parms()
   (in dialogs.cpp), FileListDialog::view_file() and
   FileListDialog::get_view_file_parms() (in filelist.cpp) and
   SocketListDialog::view_file() and
   SocketListDialog::get_view_file_parms() (in socket_list.cpp).)

   The global lock Prog_config::mutex_p is provided for the purpose.
   Global locks are quite dangerous - they can easily lead to
   deadlocks from out-of-order locking of other mutexes.  Accordingly,
   only lock Prog_config::mutex_p if no other mutex locks are being
   held.

5. See also the comments about thread sefety in the function
   write_error() in mainwindow.cpp.

6. It would have been convenient to have signal handling in a separate
   thread and then to sigwait() on the signals used by this program.
   However the main signal of interest is SIGCHLD, from which the
   termination of a send or receive session from efax can be detected,
   and information about the success or failure of the sending or
   receipt of the fax can be obtained and acted on.  Unfortunately,
   linuxthreads does not properly implement POSIX threads as regards
   signal handling, and with linuxthreads only the thread which
   fork()s will receive SIGCHLD.  Thus SIGCHLD cannot be received in a
   special signal handling thread.  Instead, a timer event handler is
   provided in MainWindow::timer_event() (which also calls
   EfaxController::timer_event()) which deals with interrupts and
   child exit handling.

*********************************************************************/

struct Prog_config {
  std::string lock_file;
  std::string fixed_font;
  std::string homedir;
  std::string working_dir;
  char rings;
  bool tone_dial;
  bool found_rcfile;
  bool GPL_flag;
  bool gtkprint;
  bool print_popup;
  bool sock_server;
  bool sock_popup;
  bool other_sock_client_address;
  bool fax_received_popup;
  bool fax_received_exec;
  bool addr_in_header;
  std::string my_name;
  std::string my_number;
  std::string page_size;  // either 'a4' 'letter' or 'legal'
  std::string page_dim;
  std::string resolution;
  std::string dial_prefix;
  std::string print_cmd;
  std::string print_shrink;
  std::string ps_view_cmd;
  std::string logfile_name;
  std::string sock_server_port;
  std::string fax_received_prog;
  std::vector<std::string> parms;
  std::vector<std::string> permitted_clients_list;
  pid_t efax_controller_child_pid;
  GobjHandle<GdkPixbuf> window_icon_h;
  Thread::Mutex* mutex_p;
};

extern Prog_config prog_config;         // defined in main.cpp
std::string configure_prog(bool reread);// also defined in main.cpp (pass reread as true if
                                        // the program parameters are being reread)

// generic beep() function defined in main.cpp
void beep(void);

// general function for writing an error defined in mainwindow.cpp
// the message must not be larger than PIPE_BUF in size.  The function
// handles EINTR - you do not need to check this yourself.  The error
// messages are put in a pipe which is non-blocking - it will be
// emptied fairly promptly but messages should be less than PIPE_BUF
// in length or any excess may be lost (it returns the number of
// bytes written, so the return value can be used to achieve long
// writes, if really wanted).
ssize_t write_error(const char*);

// this function is defined in mainwindow.cpp, and will cause any
// message written to stderr to be taken by the in built error pipe
// in MainWindow.  Only call this in a child process after fork()ing.
int connect_to_stderr(void);

#endif
