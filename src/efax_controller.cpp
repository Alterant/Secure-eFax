/* Copyright (C) 2001 to 2006 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <fstream>
#include <iomanip>
#include <memory>
#include <ctime>
#include <cstring>
#include <cstdlib>

// uncomment for debugging in EfaxController::timer_event()
//#include <iostream>

#include <glib/gtimer.h>

#include "efax_controller.h"
#include "fax_list_manager.h"
#include "utils/thread.h"
#include "utils/mutex.h"
#include "utils/shared_handle.h"
#include "utils/callback.h"

#ifdef HAVE_OSTREAM
#include <ios>
#include <ostream>
#endif

#ifdef HAVE_STRINGSTREAM
#include <sstream>
#else
#include <strstream>
#endif

#ifdef HAVE_STREAM_IMBUE
#include <locale>
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

// the headers on one of my machines does not properly include the declaration
// of getpgid() for some reason, so declare it here explicitly
extern "C" pid_t getpgid(pid_t);


EfaxController::EfaxController(void): state(inactive),
				      close_down(false),
                                      received_fax_count(0) {
  // set up state_messages

  state_messages.push_back(gettext("Inactive"));
  state_messages.push_back(gettext("Sending fax"));
  state_messages.push_back(gettext("Answering call"));
  state_messages.push_back(gettext("Answering call"));
  state_messages.push_back(gettext("Standing by to receive calls"));
  state_messages.push_back(gettext("Sending fax"));
  state_messages.push_back(gettext("Sending fax"));

  fax_made_notify.connect(sigc::mem_fun(*this, &EfaxController::sendfax_slot));
  no_fax_made_notify.connect(sigc::mem_fun(*this, &EfaxController::no_fax_made_slot));
}

void EfaxController::efax_closedown(void) {
  if (state == inactive) ready_to_quit_notify();
  else if (!close_down) {
    close_down = true;
    stop();
  }
}

void EfaxController::init_sendfax_parms(void) {

  sendfax_parms_vec = prog_config.parms;

  // now add the first set of arguments to the copy of prog_config.parms
  // in sendfax_parms_vec
     
  struct std::tm* time_p;
  std::time_t time_count;

  std::time(&time_count);
  time_p = std::localtime(&time_count);

  char date_string[150];
  // create the date and time in an internationally acceptable
  // numeric (ASCII) only character format which efax can make sense of
  const char format[] = "%Y-%m-%d %H:%M";
  std::strftime(date_string, sizeof(date_string), format, time_p);

  std::string temp("-h");
  temp += date_string;
  temp += "    ";
  temp += prog_config.my_name;

  temp += " (";
  temp += prog_config.my_number + ") --> ";
  if (prog_config.addr_in_header) temp += last_fax_item_sent.number;
  temp += "      %d/%d";
  sendfax_parms_vec.push_back(temp);

  if (last_fax_item_sent.number.empty()) {
    sendfax_parms_vec.push_back("-jX3");
    sendfax_parms_vec.push_back("-t");
    sendfax_parms_vec.push_back("");
  }
  else {
    temp = "-t";
    if (prog_config.tone_dial) temp += 'T';
    else temp += 'P';
    temp += last_fax_item_sent.number;
    sendfax_parms_vec.push_back(temp);
  }
}

void EfaxController::sendfax_impl(const Fax_item& fax_item, bool in_timer_event) {

  if (state == receive_standby) {
    if (!is_receiving_fax()) {
      last_fax_item_sent = fax_item;
      state = start_send_on_standby;
      kill_child();
    }
    else write_error(gettext("Cannot send fax - a fax is being received\n"));
  }

  else if (state == inactive || (in_timer_event && state == start_send_on_standby)) {
    stdout_pipe.open(PipeFifo::non_block);
    // efax is sensitive to timing, so set pipe write to non-block also
    stdout_pipe.make_write_non_block();
    
    if (state == inactive) {
      state = sending;
      last_fax_item_sent = fax_item;
    }
    // else state == start_send_on_standby - we don't need to assign to
    // last_fax_item_sent in this case as it has already been done in the
    // 'if' block opening this function on the previous call to this function
    else state = send_on_standby;

    display_state();

    // get the first set of arguments for the exec() call in sendfax_slot (because
    // this is a multi-threaded program, we must do this before fork()ing because
    // we use functions to get the arguments which are not async-signal-safe)
    // the arguments are loaded into the EfaxController object member sendfax_parms_vec
    init_sendfax_parms();

    // now launch a worker thread to make up the fax pages in tiffg3 format
    // for sending by efax - the thread will emit dispatcher fax_made_notify
    // if the fax is correctly made up, which will call sendfax_slot() in this
    // initial (GUI) thread, which in turn invokes efax and sends the fax.  If
    // the fax is not correctly made up, the worker thread will emit dispatcher
    // no_fax_made_notify, which will call no_fax_made_slot() in this initial
    // (GUI) thread so as to clean up correctly

    // first block off the signals for which we have set handlers so that the worker
    // thread does not receive the signals, otherwise we will have memory synchronisation
    // issues in multi-processor systems - we will unblock in the initial (GUI) thread
    // as soon as the worker thread has been launched
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &sig_mask, 0);

    std::auto_ptr<Thread::Thread> res =
      Thread::Thread::start(Callback::make(*this, &EfaxController::make_fax_thread),
			    false);
    if (!res.get()) {
      write_error("Cannot start thread to make fax for sending\n");
      stdout_pipe.close();
      state = inactive;
      display_state();
    }
    // now unblock signals so that the initial (GUI) thread can receive them
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, 0);
  }
  else beep();
}

void EfaxController::no_fax_made_slot(void) {

  // first deal with the synchronising semaphore we posted to in
  // EfaxController::make_fax_thread() - this wait cannot actually block
  // because we have already posted to fax_made_sem in that method
  fax_made_sem.wait();

  cleanup_fax_send_fail();
  stdout_pipe.close();
  State state_val = state;
  state = inactive;
  if (state_val == send_on_standby) receive(receive_standby);
  // we don't need to call display_state() if we have called receive(), as
  // receive() will call display_state() itself
  else display_state();
}

std::pair<const char*, char* const*> EfaxController::get_sendfax_parms(void) {

  // sendfax_parms_vec has already been filled by init_sendfax_parms() and by
  // make_fax_thread() - so make up the C style arrays for the execvp() call
  // in sendfax_slot()
  char** exec_parms = new char*[sendfax_parms_vec.size() + 1];

  std::vector<std::string>::const_iterator iter;
  char**  temp_pp = exec_parms;
  for (iter = sendfax_parms_vec.begin(); iter != sendfax_parms_vec.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }

  *temp_pp = 0;

  char* prog_name = new char[std::strlen("efax-0.9a") + 1];
  std::strcpy(prog_name, "efax-0.9a");

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void EfaxController::sendfax_slot(void) {

  // this method runs in the initial (GUI) thread -  we get this process
  // to run in the initial (GUI) thread by invoking it from a Notifier
  // object in EfaxController::make_fax_thread(), which will execute the
  // slot attached to that object (this method) in the thread in which the
  // Notifier object was created.  The Notifier object was
  // created as a member of the EfaxController object, and thus was created
  // in the initial (GUI) thread.  We need to execute this method in the GUI
  // thread because the program calls waitpid() in that thread to examine
  // exit status of the child process this method creates with fork(), and
  // with Linuxthreads this will not otherwise work correctly

  // first deal with the synchronising semaphore we posted to in
  // EfaxController::make_fax_thread() - this wait cannot actually block
  // because we have already posted to fax_made_sem in that method
  fax_made_sem.wait();

  // get the arguments for the exec() call below (because this is a
  // multi-threaded program, we must do this before fork()ing because
  // we use functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> sendfax_parms(get_sendfax_parms());

  // set up a synchronising pipe  in case the child process finishes before
  // fork() in parent space has returned (yes, with an exec() error that can
  // happen with Linux kernel 2.6)
  SyncPipe sync_pipe;

  prog_config.efax_controller_child_pid = fork();

  if (prog_config.efax_controller_child_pid == -1) {
    write_error("Fork error - exiting\n");
    std::exit(FORK_ERROR);
  }
  if (!prog_config.efax_controller_child_pid) {  // child process - as soon as everything is set up we are going to do an exec()

    // now we have forked, we can connect stdout_pipe to stdout
    // and connect MainWindow::error_pipe to stderr
    stdout_pipe.connect_to_stdout();
    connect_to_stderr();

    // wait before we call execvp() until the parent process has set itself up
    // and releases this process
    sync_pipe.wait();

    execvp(sendfax_parms.first, sendfax_parms.second);

    // if we reached this point, then the execvp() call must have failed
    // report error and exit - uses _exit() and not exit()
    write_error("Can't find the efax-0.9a program - please check your installation\n"
		"and the PATH environmental variable\n");
    _exit(EXEC_ERROR); 
  } // end of child process
    
  // this is the parent process
  stdout_pipe.make_readonly();   // since the pipe is unidirectional, we can close the write fd
  join_child();

  // now we have set up, release the child process
  sync_pipe.release();
    
  // release the memory allocated on the heap for
  // the redundant sendfax_parms_pair
  // we are in the main parent process here - no worries about
  // only being able to use async-signal-safe functions
  delete_parms(sendfax_parms);
}

std::pair<const char*, char* const*> EfaxController::get_gs_parms(const std::string& basename) {

  std::vector<std::string> parms;

  { // scope block for mutex lock
    // lock the Prog_config object to stop it being modified in the intial (GUI) thread
    // while we are accessing it here
    Thread::Mutex::Lock lock(*prog_config.mutex_p);

    std::string temp;
    parms.push_back("gs");
    parms.push_back("-q");
    parms.push_back("-sDEVICE=tiffg3");
    temp = "-r";
    temp += prog_config.resolution;
    parms.push_back(temp);
    parms.push_back("-dNOPAUSE");
    parms.push_back("-dSAFER");
    temp = "-sOutputFile=";
    temp += basename + ".%03d";
    parms.push_back(temp);
    temp = "-sPAPERSIZE=";
    temp += prog_config.page_size;
    parms.push_back(temp);
    parms.push_back(basename);
  }

  char** exec_parms = new char*[parms.size() + 1];

  std::vector<std::string>::const_iterator iter;
  char**  temp_pp = exec_parms;
  for (iter = parms.begin(); iter != parms.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }

  *temp_pp = 0;
  
  char* prog_name = new char[std::strlen("gs") + 1];
  std::strcpy(prog_name, "gs");

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void EfaxController::make_fax_thread(void) {
  // convert the postscript file(s) into tiffg3 fax files, beginning at [filename].001
  // we will use ghostscript.  we will also load the results into sendfax_parms_vec

  std::vector<std::string>::const_iterator filename_iter;

  // we do not need a mutex to protect last_fax_item_sent - until EfaxController::timer_event()
  // resets state to inactive or receive_standby, we cannot invoke sendfax() again
  for (filename_iter = last_fax_item_sent.file_list.begin();
       filename_iter != last_fax_item_sent.file_list.end(); ++filename_iter) {

    std::string::size_type pos = filename_iter->find_last_of('/');

    if (pos == std::string::npos || pos + 2 > filename_iter->size()) {
      write_error(gettext("Not valid file name\n"));

      // synchronise memory on multi-processor systems before we emit the
      // Notifier signal - we call fax_made_sem.wait() in
      // EfaxController::no_fax_made_slot()
      fax_made_sem.post();

      // clean up and then end this worker thread
      no_fax_made_notify();
      return;
    }
    
    else if (access(filename_iter->c_str(), F_OK)) {
      write_error(gettext("File does not exist\n"));

      // synchronise memory on multi-processor systems before we emit the
      // Notifier signal - we call fax_made_sem.wait() in
      // EfaxController::no_fax_made_slot()
      fax_made_sem.post();

      // clean up and then end this worker thread
      no_fax_made_notify();
      return;
    }

    else if (access(filename_iter->c_str(), R_OK)) {
      write_error(gettext("User does not have read permission on the file\n"));

      // synchronise memory on multi-processor systems before we emit the
      // Notifier signal - we call fax_made_sem.wait() in
      // EfaxController::no_fax_made_slot()
      fax_made_sem.post();

      // clean up and then end this worker thread
      no_fax_made_notify();
      return;
    }

    // unfortunately ghostscript does not handle long file names
    // so we need to separate the file name from the full path (we will chdir() to the directory later)
    // pos is already set to the position of the last '/' character
    std::string dirname(filename_iter->substr(0, pos));
    pos++;
    std::string basename(filename_iter->substr(pos));

    // get the arguments for the exec() call below (because this is a
    // multi-threaded program, we must do this before fork()ing because
    // we use functions to get the arguments which are not async-signal-safe)
    std::pair<const char*, char* const*> gs_parms(get_gs_parms(basename));

    // create a synchronising pipe - we need to wait() on gs having completed executing
    // and then notify the parent process.  To wait() successfully we need to fork() once,
    // reset the child signal handler, and then fork() again
    SyncPipe sync_pipe;

    pid_t pid = fork();
    
    if (pid == -1) {
      write_error("Fork error\n");
      std::exit(FORK_ERROR);
    }
    if (!pid) { // child process

      // unblock signals as these are blocked for all worker threads
      // (the child process inherits the signal mask of the thread
      // creating it with the fork() call)
      sigset_t sig_mask;
      sigemptyset(&sig_mask);
      sigaddset(&sig_mask, SIGCHLD);
      sigaddset(&sig_mask, SIGQUIT);
      sigaddset(&sig_mask, SIGTERM);
      sigaddset(&sig_mask, SIGINT);
      sigaddset(&sig_mask, SIGHUP);
      // this child process is single threaded, so we can use sigprocmask()
      // rather than pthread_sigmask() (and should do so as sigprocmask()
      // is guaranteed to be async-signal-safe)
      // this process will not be receiving interrupts so we do not need
      // to test for EINTR on the call to sigprocmask()
      sigprocmask(SIG_UNBLOCK, &sig_mask, 0);

      connect_to_stderr();

      // now fork() again
      pid_t pid = fork();

      if (pid == -1) {
	write_error("Fork error\n");
	_exit(FORK_ERROR); // we have already forked, so use _exit() not exit()
      }
      if (!pid) {  // child process - when everything is set up, we are going to do an exec()

	// we don't need sync_pipe in this process - ignore it by calling release()
	sync_pipe.release();

	// now start up ghostscript
	// first we need to connect stdin to /dev/null to make ghostscript terminate
	// this process will not be receiving interrupts so we do not need
	// to test for EINTR on the calls to open(), dup2() and close() as this
	// process will not be receiving any signals
	int fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
	  write_error("Cannot open /dev/null in EfaxController::make_fax_thread()\n");
	  // in case of error end child process here
	  _exit(FILEOPEN_ERROR);
	}
	dup2(fd, 0);
	// now close stdout
	dup2(fd, 1);
	close(fd); // now stdin and stdout read/write to /dev/null, we can close the /dev/null file descriptor

	// unfortunately ghostscript does not handle long file names
	// so we need to chdir()
	chdir(dirname.c_str());
	execvp(gs_parms.first, gs_parms.second);

	// if we reached this point, then the execvp() call must have failed
	write_error("Can't find the ghostscript program - please check your installation\n"
		    "and the PATH environmental variable\n");
	// this child process must end here - use _exit() not exit()
	_exit(EXEC_ERROR); 
      } // end of child process

      // this is the parent process

      // wait until ghostscript has produced the fax tiffg3 fax file.
      // Note that we have already fork()ed so this process will not be
      // receiving signals so we do not need to test for EINTR on the
      // call to wait()
      wait(0);

      // release the waiting parent process
      sync_pipe.release();

      // now end the process - use _exit() not exit()
      _exit(0);
    }
    // wait on sync_pipe until we know gs has made all the files in tiffg3 format
    sync_pipe.wait();

    // release the memory allocated on the heap for
    // the redundant gs_parms
    // we are in the main parent process here - no worries about
    // only being able to use async-signal-safe functions
    delete_parms(gs_parms);

    // now enter the names of the created files in sendfax_parms_vec
    bool valid_file = false;
    int partnumber = 1;
#ifdef HAVE_STRINGSTREAM
    std::ostringstream strm;

#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

    strm << *filename_iter << '.' << std::setfill('0')
	 << std::setw(3) << partnumber;
    int result = access(strm.str().c_str(), R_OK);
    
    while (!result) {  // file OK
      // valid_file only needs to be set true once, but it is more
      // convenient to do it in this loop
      valid_file = true;

      // we do not need a mutex to protect sendfax_parms_vec - until
      // EfaxController::timer_event() resets state to inactive or
      // receive_standby, we cannot invoke sendfax() again
      sendfax_parms_vec.push_back(strm.str());

      partnumber++;
      strm.str("");
      strm << *filename_iter << '.' << std::setfill('0')
	   << std::setw(3) << partnumber;
      result = access(strm.str().c_str(), R_OK);
    }
#else
    std::ostrstream strm;

#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

    strm << filename_iter->c_str() << '.' << std::setfill('0')
	 << std::setw(3) << partnumber << std::ends;
    const char* test_name = strm.str();
    int result = access(test_name, R_OK);
  
    while (!result) {  // file OK
      // valid_file only needs to be set true once, but it is more
      // convenient to do it in this loop
      valid_file = true;

      sendfax_parms_vec.push_back(test_name);
      delete[] test_name;

      partnumber++;
      std::ostrstream strm;

#  ifdef HAVE_STREAM_IMBUE
      strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

      strm << filename_iter->c_str() << '.' << std::setfill('0')
	   << std::setw(3) << partnumber << std::ends;
      test_name = strm.str();
      result = access(test_name, R_OK);
    }
    delete[] test_name;
#endif  
    
    if (!valid_file) {
      write_error(gettext("Not valid postscript file\n"));

      // synchronise memory on multi-processor systems before we emit the
      // Notifier signal - we call fax_made_sem.wait() in
      // EfaxController::no_fax_made_slot()
      fax_made_sem.post();

      // clean up and then end this worker thread
      no_fax_made_notify();
      return;
    }
  }
  // we have now made the fax pages in tiffg3 format and entered them into sendfax_parms_vec.
  // At the end of this method this worker thread will end, and emitting fax_made_notify
  // will cause sendfax_slot() to be executed in the initial (GUI) thread

  // synchronise memory on multi-processor systems before we emit the
  // Notifier signal - we call fax_made_sem.wait() in
  // EfaxController::sendfax_slot()
  fax_made_sem.post();

  fax_made_notify();
}

std::pair<const char*, char* const*> EfaxController::get_receive_parms(int mode) {

  std::vector<std::string> efax_parms(prog_config.parms);
  std::string temp;

  efax_parms.push_back("-rcurrent");

  if (mode == receive_takeover) efax_parms.push_back("-w");
      
  else if (mode == receive_standby) {
    temp = "-jS0=";
    temp += prog_config.rings;
    efax_parms.push_back(temp);
    efax_parms.push_back("-s");
    efax_parms.push_back("-w");
  }
      
  char** exec_parms = new char*[efax_parms.size() + 1];

  std::vector<std::string>::const_iterator iter;
  char**  temp_pp = exec_parms;
  for (iter = efax_parms.begin(); iter != efax_parms.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }
  
  *temp_pp = 0;

  char* prog_name = new char[std::strlen("efax-0.9a") + 1];
  std::strcpy(prog_name, "efax-0.9a");
  
  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void EfaxController::receive(State mode) {

  // check pre-conditions
  if (state != inactive) {
    beep();
    return;
  }
  if (prog_config.working_dir.empty()) {
    write_error(gettext("You don't have the $HOME environmental variable set\n"));
    beep();
    return;
  }

  // now proceed to put the program in receive mode
  std::string fax_pathname(prog_config.working_dir);
  fax_pathname += "/faxin/current";
  // we can extract the C string here - we do not do anything below to make it invalid
  const char* fax_pathname_str = fax_pathname.c_str();

  if (mkdir(fax_pathname_str, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
    write_error(gettext("SERIOUS SYSTEM ERROR: Can't create directory to save received faxes.\n"
			"Faxes cannot be received!\n"));
  }
  else {

    stdout_pipe.open(PipeFifo::non_block);
    // efax is sensitive to timing, so set pipe write to non-block also
    stdout_pipe.make_write_non_block();
  
    state = mode;
    display_state();

    // get the arguments for the exec() call below (because this is a
    // multi-threaded program, we must do this before fork()ing because
    // we use functions to get the arguments which are not async-signal-safe)
    std::pair<const char*, char* const*> receive_parms(get_receive_parms(mode));

    // set up a synchronising pipe  in case the child process finishes before
    // fork() in parent space has returned (yes, with an exec() error that can
    // happen with Linux kernel 2.6)
    SyncPipe sync_pipe;

    prog_config.efax_controller_child_pid = fork();
    if (prog_config.efax_controller_child_pid == -1) {
      write_error("Fork error - exiting\n");
      std::exit(FORK_ERROR);
    }
    if (!prog_config.efax_controller_child_pid) {  // child process - as soon as everything is set up we are going to do an exec()

      // now we have forked, we can connect stdout_pipe to stdout
      // and connect MainWindow::error_pipe to stderr
      stdout_pipe.connect_to_stdout();
      connect_to_stderr();

      chdir(fax_pathname_str);

      // wait before we call execvp() until the parent process has set itself up
      // and releases this process
      sync_pipe.wait();

      // now start up efax in receive mode
      execvp(receive_parms.first, receive_parms.second);

      // if we reached this point, then the execvp() call must have failed
      // report the error and end this process - use _exit() and not exit()
      write_error("Can't find the efax-0.9a program - please check your installation\n"
		  "and the PATH environmental variable\n");
      _exit(EXEC_ERROR); 
    } // end of child process

    // this is the parent process
    stdout_pipe.make_readonly();   // since the pipe is unidirectional, we can close the write fd
    join_child();

    // now we have set up, release the child process
    sync_pipe.release();

    // release the memory allocated on the heap for
    // the redundant receive_parms
    // we are in the main parent process here - no worries about
    // only being able to use async-signal-safe functions
    delete_parms(receive_parms);
  }
}

void EfaxController::delete_parms(std::pair<const char*, char* const*> parms_pair) {

  delete[] parms_pair.first;
  char* const* temp_pp = parms_pair.second;
  for(; *temp_pp != 0; ++temp_pp) {
    delete[] *temp_pp;
  }
  delete[] parms_pair.second;
}

void EfaxController::stop(void) {
 if (state != inactive) {
   stdout_message(gettext("\n*** Stopping send/receive session ***\n\n"));
   kill_child();
 }
  else beep();
}

void EfaxController::timer_event(void) {

  // if a fax list is in the process of being constructed, we need to wait until
  // construction has completed before reaping efax exit status and acting on it
  // (we might have reached here through the call to gtk_main_iteration() in
  // FaxListManager::populate_fax_list())
  if (FaxListManager::is_fax_received_list_main_iteration()
      || FaxListManager::is_fax_sent_list_main_iteration()) {
    return;
  }

  // preconditions are OK - proceed

  int stat_val;
  pid_t result;

  // reap the status of any exited child processes
  while ((result = waitpid(-1, &stat_val, WNOHANG)) > 0) {

    if (result && result == prog_config.efax_controller_child_pid) {

      prog_config.efax_controller_child_pid = 0;

      int exit_code = -1;
      if (WIFEXITED(stat_val)) exit_code = WEXITSTATUS(stat_val);

      // this won't work if efax is suid root and we are not root
      if (!prog_config.lock_file.empty()) unlink(prog_config.lock_file.c_str());
    
      bool restart_standby = false;
      bool end_receive = false;

      switch(state) {

      case EfaxController::sending:
      case EfaxController::send_on_standby:
	if (!exit_code) {
	  std::pair<std::string, std::string> fax_info(save_sent_fax());
	  // notify that the fax has been sent
	  if (!fax_info.first.empty()) fax_sent_notify(fax_info);

	  if (last_fax_item_sent.is_socket_file) {
	    // if we are sending a print job via the socket server, notify the
	    // SocketServer object which created the temporary file (the server
	    // object will clean up by deleting it and also cause the socket
	    // file list to be updated - we do not need to do that here)
	    remove_from_socket_server_filelist(last_fax_item_sent.file_list[0]);
	  }
	}
	else cleanup_fax_send_fail();
	unjoin_child();
	{
	  State state_val = state;
	  state = inactive;
	  if (state_val == send_on_standby
	      && !close_down) {
	    receive(receive_standby);
	  }
	  // we do not need to call display_state() if we have called receive(), as
	  // receive() will call display_state() itself
	  else display_state();
	}
	break;
      case EfaxController::start_send_on_standby:
	receive_cleanup();
	unjoin_child();
	if (!close_down) sendfax_impl(last_fax_item_sent, true);
	else {
	  state = inactive;
	  display_state();
	}
	break;
      case EfaxController::receive_standby:
	if ((!exit_code || exit_code == 3)
	    && !close_down) {
	  restart_standby = true;
	}
	else end_receive = true;
	break;
      default:
	if (!inactive) end_receive = true;
	break;
      }

      if (end_receive || restart_standby) {
	receive_cleanup();
	unjoin_child();
	state = inactive; // this is needed even if we are going to call receive()
	// now restart if in standby mode
	if (restart_standby) receive(receive_standby);
	// we do not need to call display_state() if we have called receive(), as
	// receive() will call display_state() itself (calling display_state() will
	// also update the received fax count displayed in the tray icon tooltip if we are
	// in receive_standby mode - this will appear in the tray_item tooltip, because
	// TrayIcon::set_tooltip_slot() is connected to the write_state signal in the
	// MainWindow::MainWindow() constructor)
	else display_state();
	restart_standby = false;
	end_receive = false;
      }

      if (close_down && state == inactive) ready_to_quit_notify();
    }
    /* uncomment for debugging
    else {
      std::cout << "Not efax_controller child" << std::endl;
      std::cout << "Child pid is " << result << std::endl;
      std::cout << "Child exit code is " << exit_code << std::endl;
    }
    */
  }
}

void EfaxController::receive_cleanup(void) {
  // delete the "current" receive directory if it is empty
  std::string full_dirname(prog_config.working_dir);
  full_dirname += "/faxin/current";
  int result = rmdir(full_dirname.c_str()); // only deletes the directory if it is empty

  if (result == -1 && errno == ENOTEMPTY) {
    // before assuming a fax has been successfully received, check to see
    // if current.001 is of 0 size (if it is delete it)
    struct stat statinfo;
    std::string file_name(full_dirname);
    file_name += "/current.001";
    if (!stat(file_name.c_str(), &statinfo)
	&& !statinfo.st_size) {
      unlink(file_name.c_str());
      rmdir(full_dirname.c_str());
    }
    else { // we have received a fax - save it
      std::pair<std::string, std::string> fax_info(save_received_fax());
      // notify that the fax has been received
      if (!fax_info.first.empty()) {
	received_fax_count++;
	fax_received_notify(fax_info);
      }
    }
  }
}

std::pair<std::string, std::string> EfaxController::save_received_fax(void) {

  char faxname[18];
  *faxname = 0;

  // get a time value to create the directory name into which the fax is to be saved
  struct std::tm* time_p;
  std::time_t time_count;
    
  std::time(&time_count);
  time_p = std::localtime(&time_count);

  std::string new_dirname(prog_config.working_dir);
  new_dirname += "/faxin/";

  const char format[] = "%Y%m%d%H%M%S";
  std::strftime(faxname, sizeof(faxname), format, time_p);
  std::string temp(new_dirname);
  temp += faxname;

  // check whether directory already exists or can't be created
  int count;
  for (count = 0; count < 4 && mkdir(temp.c_str(), S_IRUSR | S_IWUSR | S_IXUSR); count++) {
    // it is not worth checking for a short sleep because of a signal as we will
    // repeat the attempt four times anyway
    sleep(1); // wait a second to get a different time
    std::time(&time_count);
    time_p = std::localtime(&time_count);
    std::strftime(faxname, sizeof(faxname), format, time_p);
    temp = new_dirname + faxname;
  }
  if (count == 4) {
    write_error(gettext("SERIOUS SYSTEM ERROR: Can't create directory to save received fax.\n"
			"This fax will not be saved!\n"));
    *faxname = 0;
  }

  else { // move the faxes into the new directory

    // make new_dirname the same as the directory name for the saved faxes
    // that we have just created
    new_dirname += faxname;

    std::string old_dirname(prog_config.working_dir);
    old_dirname += "/faxin/current";

    std::vector<std::string> filelist;
    struct dirent* direntry;
    struct stat statinfo;

    DIR* dir_p;
    if ((dir_p = opendir(old_dirname.c_str())) == 0) {
      write_error(gettext("SERIOUS SYSTEM ERROR: Can't open received fax directory.\n"
			  "This fax will not be saved!\n"));
    }

    else {
      chdir(old_dirname.c_str());
      while ((direntry = readdir(dir_p)) != 0) {
	stat(direntry->d_name, &statinfo);
	if (S_ISREG(statinfo.st_mode)) {
	  filelist.push_back(direntry->d_name);
	}
      }

      closedir(dir_p);

      bool failure = false;
      std::vector<std::string>::const_iterator iter;
      std::string old_filename;
      std::string new_filename;

      for (iter = filelist.begin(); iter != filelist.end(); ++iter) {
	old_filename = old_dirname;
	old_filename += '/';
	old_filename += *iter;
	if (!iter->substr(0, std::strlen("current.")).compare("current.")) {
	  new_filename = new_dirname;
	  new_filename += '/';
	  new_filename += faxname;
	  new_filename += iter->substr(std::strlen("current"));  // add the suffix (".001" etc.)
	  if (link(old_filename.c_str(), new_filename.c_str())) {
	    failure = true;
	    break;
	  }
	}
      }
      if (failure) {
	write_error(gettext("SERIOUS SYSTEM ERROR: Cannot save all of the received fax.\n"
			    "All or part of the fax will be missing!\n"));
      }
      else {
	for (iter = filelist.begin(); iter != filelist.end(); ++iter) {
	  old_filename = old_dirname;
	  old_filename += '/';
	  old_filename += *iter;
	  unlink(old_filename.c_str());
	}
	if (rmdir(old_dirname.c_str())) {
	  std::string msg("Can't delete directory ");
	  msg += old_dirname;
	  msg += "\nThe contents should have been moved to ";
	  msg += new_dirname;
	  msg += "\nand it should now be empty -- please check\n";
	  write_error(msg.c_str());
	}
      }	
    }

    // reset current directory
    std::string temp(prog_config.working_dir + "/faxin");
    chdir(temp.c_str());
  }
  // this program does not provide an automatic fax description at the moment
  // so pass an empty string as the secong member of the pair
  return std::pair<std::string, std::string>(std::string(faxname), std::string());
}

bool EfaxController::read_pipe_slot(void) {

  char pipe_buffer[PIPE_BUF + 1];
  ssize_t result;

  while ((result = stdout_pipe.read(pipe_buffer, PIPE_BUF)) > 0) {
    SharedHandle<char*> output_h(stdout_pipe_reassembler(pipe_buffer, result));
    if (output_h.get()) stdout_message(output_h.get());
    else write_error(gettext("Invalid Utf8 received in EfaxController::read_pipe_slot()\n"));
  }
  return true; // this is multi-shot
}

void EfaxController::join_child(void) {
  // we do not need a mutex for this - join_child() and unjoin_child() are
  // only called in the initial (GUI) thread

  iowatch_tag = start_iowatch(stdout_pipe.get_read_fd(),
			      sigc::mem_fun(*this, &EfaxController::read_pipe_slot),
			      G_IO_IN);
}

void EfaxController::unjoin_child(void) {
  // we do not need a mutex for this - join_child() and unjoin_child() are
  // only called in the initial (GUI) thread, and stdout_pipe is only manipulated
  // in that thread or after a new single-threaded process has been launched with fork()

  g_source_remove(iowatch_tag);
  stdout_pipe.close();
  stdout_pipe_reassembler.reset();
}

void EfaxController::kill_child(void) {
  // it shouldn't be necessary to check whether the process group id of
  // the process whose pid is in prog_config.efax_controller_child_pid
  // is the same as efax-gtk (that is, prog_config.efax_controller_child_pid
  // is not out of date) because it shouldn't happen - even if efax has ended
  // we cannot have reaped its exit value yet if prog_config.efax_controller_child_pid
  // is not 0 so it should still be a zombie in the process table, but for
  // safety's sake let's just check it
  if (prog_config.efax_controller_child_pid > 0
      && getpgid(0) == getpgid(prog_config.efax_controller_child_pid)) {
    kill(prog_config.efax_controller_child_pid, SIGTERM);
    // one second delay
    g_usleep(1000000);
    // now really make sure (we don't need to check
    // prog_config.efax_controller_child_pid again in the
    // implementation as at version 2.2.12 because the child exit
    // handling is in this thread, via MainWindow::timer_event_handler()
    // and EfaxController::timer_event() but we would need to check it
    // again (and protect it with a mutex) if the program is modified
    // to put child exit handling in a signal handling thread waiting
    // on sigwait() which resets the value of
    // prog_config.efax_controller_child_pid)
    kill(prog_config.efax_controller_child_pid, SIGKILL);
  }
}

std::pair<std::string, std::string> EfaxController::save_sent_fax(void) {

  char faxname[18];
  *faxname = 0;
  std::string description;

  // get a time value to create the directory name into which the fax is to be saved
  struct std::tm* time_p;
  std::time_t time_count;
    
  std::time(&time_count);
  time_p = std::localtime(&time_count);

  // now create the directory into which the fax files to be saved
  std::string fileout_name(prog_config.working_dir);
  fileout_name += "/faxsent/";

  const char dirname_format[] = "%Y%m%d%H%M%S";
  std::strftime(faxname, sizeof(faxname), dirname_format, time_p);
  std::string temp(fileout_name);
  temp += faxname;

  // check whether directory already exists or can't be created
  int count;
  for (count = 0; count < 4 && mkdir(temp.c_str(), S_IRUSR | S_IWUSR | S_IXUSR); count++) {
    // it is not worth checking for a short sleep because of a signal as we will
    // repeat the attempt four times anyway
    sleep(1); // wait a second to get a different time
    std::time(&time_count);
    time_p = std::localtime(&time_count);
    std::strftime(faxname, sizeof(faxname), dirname_format, time_p);
    temp = fileout_name + faxname;
  }
  if (count == 4) {
    write_error(gettext("SERIOUS SYSTEM ERROR: Can't create directory to save sent fax.\n"
			"This fax will not be saved!\n"));
    *faxname = 0;
  }

  else {
    // now make fileout_name the same as the directory name for the saved faxes
    // that we have just created
    fileout_name += faxname;
    const std::string fileout_dirname(fileout_name); // keep for later to enter a description

    // and now complete the unsuffixed file name of the destination files
    fileout_name += '/';
    fileout_name += faxname;

    // now create a string containing the date for the fax description
    // glibc has const struct tm* as last param of strftime()
    // but the const does not appear to be guaranteed by POSIX
    // so do localtime() again just in case
    time_p = std::localtime(&time_count);
    const char date_description_format[] = "(%H%M %Z %d %b %Y)";
    const int max_description_datesize = 126;
    char date_description[max_description_datesize];
    std::strftime(date_description, max_description_datesize, date_description_format, time_p);

    // now copy files into the relevant directory for the fax pages to be saved

    std::vector<std::string> file_basename_vec; // this is used in making the fax description
    int out_partnumber = 1; // in order to generate the suffix for each fax page as saved
    
    std::vector<std::string>::const_iterator sentname_iter;
    for (sentname_iter = last_fax_item_sent.file_list.begin();
	 sentname_iter != last_fax_item_sent.file_list.end(); ++sentname_iter) {

      std::string::size_type pos = sentname_iter->find_last_of('/');
      if (pos == std::string::npos || pos + 2 > sentname_iter->size()) {
	write_error("Not valid file name to save -- can't save sent fax\n");
      }

      else {
	// save the file base name for later use in making the fax description
	pos++;
	file_basename_vec.push_back(sentname_iter->substr(pos));

	// make the suffixes
	int in_partnumber = 1;
#ifdef HAVE_STRINGSTREAM
	std::ostringstream in_suffix_strm;

#  ifdef HAVE_STREAM_IMBUE
	in_suffix_strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

	in_suffix_strm << '.' << std::setfill('0')
		       << std::setw(3) << in_partnumber;

	std::ostringstream out_suffix_strm;

#  ifdef HAVE_STREAM_IMBUE
	out_suffix_strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

	out_suffix_strm << '.' << std::setfill('0')
			<< std::setw(3) << out_partnumber;

	// make the suffixed source and destination files
	std::string suffixed_inname = *sentname_iter + in_suffix_strm.str();
	std::string suffixed_outname = fileout_name + out_suffix_strm.str();

#else
	std::ostrstream in_suffix_strm;

#  ifdef HAVE_STREAM_IMBUE
	in_suffix_strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

	in_suffix_strm << '.' << std::setfill('0')
		       << std::setw(3) << in_partnumber << std::ends;
	const char* in_suffix = in_suffix_strm.str();

	std::ostrstream out_suffix_strm;

#  ifdef HAVE_STREAM_IMBUE
	out_suffix_strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

	out_suffix_strm << '.' << std::setfill('0')
			<< std::setw(3) << out_partnumber << std::ends;
	const char* out_suffix = out_suffix_strm.str();

	// make the suffixed source and destination files
	std::string suffixed_inname = *sentname_iter + in_suffix;
	std::string suffixed_outname = fileout_name + out_suffix;
	
	delete[] in_suffix;
	delete[] out_suffix;
#endif

	std::ifstream filein;
	std::ofstream fileout;
	const int BLOCKSIZE = 1024;
	char block[BLOCKSIZE];

	while (!access(suffixed_inname.c_str(), R_OK)
	       && (filein.open(suffixed_inname.c_str(), std::ios::in), filein) // use comma operator to check filein
	       && (fileout.open(suffixed_outname.c_str(), std::ios::out), fileout)) { // ditto for fileout
	  while (filein) {
	    filein.read(block, BLOCKSIZE);
	    fileout.write(block, filein.gcount());
	  }
	  filein.close();
	  filein.clear();
	  fileout.close();
	  fileout.clear();
	  unlink(suffixed_inname.c_str());
     
	  in_partnumber++;
	  out_partnumber++;
#ifdef HAVE_STRINGSTREAM
	  in_suffix_strm.str("");
	  in_suffix_strm << '.' << std::setfill('0')
			 << std::setw(3) << in_partnumber;

	  out_suffix_strm.str("");
	  out_suffix_strm << '.' << std::setfill('0')
			  << std::setw(3) << out_partnumber;

	  // make the suffixed source and destination files
	  suffixed_inname = *sentname_iter + in_suffix_strm.str();
	  suffixed_outname = fileout_name + out_suffix_strm.str();

#else
	  std::ostrstream in_suffix_strm;

#  ifdef HAVE_STREAM_IMBUE
	  in_suffix_strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

	  in_suffix_strm << '.' << std::setfill('0')
			 << std::setw(3) << in_partnumber << std::ends;
	  in_suffix = in_suffix_strm.str();

	  std::ostrstream out_suffix_strm;

#  ifdef HAVE_STREAM_IMBUE
	  out_suffix_strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

	  out_suffix_strm << '.' << std::setfill('0')
			  << std::setw(3) << out_partnumber << std::ends;
	  out_suffix = out_suffix_strm.str();

	  // make the suffixed source and destination files
	  suffixed_inname = *sentname_iter + in_suffix;
	  suffixed_outname = fileout_name + out_suffix;

	  delete[] in_suffix;
	  delete[] out_suffix;
#endif
	}
	fileout.clear();
	if (in_partnumber < 2) write_error("There was a problem saving all or part of the sent fax\n");
      }
    }

    // now save the sent fax description
    if (out_partnumber > 1) {
      const std::string description_filename(fileout_dirname + "/Description");
      std::ofstream fileout(description_filename.c_str(), std::ios::out);
      if (fileout) {
	if (last_fax_item_sent.is_socket_file) description = gettext("PRINT JOB");
	else {
	  std::vector<std::string>::const_iterator filename_iter;
	  for (filename_iter = file_basename_vec.begin();
	       filename_iter != file_basename_vec.end(); ++filename_iter) {
      
	    if (filename_iter != file_basename_vec.begin()) description += '+';
	    try {
	      description += Utf8::filename_to_utf8(*filename_iter);
	    }
	    catch (Utf8::ConversionError&) {
	      write_error("UTF-8 conversion error in EfaxController::save_sent_fax()\n");
	    }
	  }
	}
	if (!last_fax_item_sent.number.empty()) {
	  description += " --> ";
	  description += last_fax_item_sent.number;
	}
	description += ' ';
	try {
	  description += Utf8::locale_to_utf8(date_description);
	}
	catch (Utf8::ConversionError&) {
	  write_error("UTF-8 conversion error in EfaxController::save_sent_fax()\n");
	}
	fileout << description;
      }
      else write_error("Cannot save sent fax description\n");
    }
  }
  return std::pair<std::string, std::string>(std::string(faxname), description);
}

void EfaxController::cleanup_fax_send_fail(void) {

  std::vector<std::string>::const_iterator filename_iter;
  for (filename_iter = last_fax_item_sent.file_list.begin();
       filename_iter != last_fax_item_sent.file_list.end(); ++filename_iter) {

    // make the suffix
    int partnumber = 1;
#ifdef HAVE_STRINGSTREAM
    std::ostringstream strm;

#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

    strm << '.' << std::setfill('0') << std::setw(3) << partnumber;

    // make the suffixed file name
    std::string filename = *filename_iter + strm.str();

    while (!access(filename.c_str(), R_OK)) {
      unlink(filename.c_str());
      partnumber++;
      strm.str("");
      strm << '.' << std::setfill('0') << std::setw(3) << partnumber;
      filename = *filename_iter + strm.str();
    }
#else
    std::ostrstream strm;

#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

    strm << '.' << std::setfill('0') << std::setw(3) << partnumber << std::ends;
    const char* suffix = strm.str();

    // make the suffixed file name
    std::string filename = *filename_iter + suffix;
    delete[] suffix;

    while (!access(filename.c_str(), R_OK)) {
      unlink(filename.c_str());
      partnumber++;
      std::ostrstream strm;

#  ifdef HAVE_STREAM_IMBUE
      strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

      strm << '.' << std::setfill('0') << std::setw(3) << partnumber << std::ends;
      suffix = strm.str();
      filename = *filename_iter + suffix;
      delete[] suffix;
    }
#endif
  }
}

bool EfaxController::is_receiving_fax(void) const {

  bool return_val = false;

  std::string file_name(prog_config.working_dir);
  file_name += "/faxin/current/current.001";

  struct stat statinfo;
  if (!stat(file_name.c_str(), &statinfo)) return_val = true;

  return return_val;
}
