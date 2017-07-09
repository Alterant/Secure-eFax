/* Copyright (C) 2001 to 2007 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#include "prog_defs.h"

// something in gtkprintjob.h clashes with the C++ headers with
// libstdc++-5 and it needs to be included here to obtain a
// clean compile
#include <gtk/gtkversion.h>
#if GTK_CHECK_VERSION(2,10,0)
#include <gtk/gtkprintjob.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <exception>
#include <cstring>
#include <ctime>

#ifdef HAVE_OSTREAM
#include <ios>
#include <ostream>
#endif

#ifdef HAVE_ISTREAM
#include <ios>
#include <istream>
#endif

#ifdef HAVE_STRINGSTREAM
#include <sstream>
#else
#include <strstream>
#endif

#ifdef HAVE_STREAM_IMBUE
#include <locale>
#endif

#include <gtk/gtkmain.h>

#include <gdk/gdkpixbuf.h>
#include <gdk/gdk.h>   // for gdk_notify_startup_complete() and gdk_beep()

#include <glib/gutils.h>
#include <glib/gthread.h>

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#include <gdk/gdkx.h>  // for GDK_DISPLAY()
#endif

#if GTK_CHECK_VERSION(2,4,0)
#  define EFAX_GTK_USE_ICON_THEME 1
#else
#  undef EFAX_GTK_USE_ICON_THEME
#endif

#if GTK_CHECK_VERSION(2,2,0)
#  define HAVE_NOTIFY_STARTUP 1
#else
#  undef HAVE_NOTIFY_STARTUP
#endif

#ifdef EFAX_GTK_USE_ICON_THEME
#include <gtk/gtkicontheme.h>
#include "utils/icon_info_handle.h"
#endif

#include "mainwindow.h"
#include "dialogs.h"
#include "window_icon.h"
#include "utils/mutex.h"
#include "utils/utf8_utils.h"
#include "utils/gobj_handle.h"
#include "utils/fdstream.h"
#include "utils/pipes.h"
#include "utils/notifier.h"
#include "utils/gerror_handle.h"

#ifdef ENABLE_NLS
#include <locale.h>
#include <libintl.h>
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#define LOCKFILE_NAME "/.efax-gtk-pid"

// the headers on one of my machines does not properly include the declaration
// of getpgid() for some reason, so declare it here explicitly
extern "C" pid_t getpgid(pid_t);

Prog_config prog_config;

bool is_program_running(void);
bool get_prog_parm(const char*, std::string&, std::string&, std::string(*)(const std::string&));

inline bool get_prog_parm(const char* name, std::string& line, std::string& result) {
  return get_prog_parm(name, line, result, Utf8::locale_to_utf8);
}

inline bool not_ascii_char(char c) {
  if (static_cast<signed char>(c) < 0) return true;
  return false;
}

void get_fonts(void);
void get_window_icon(void);
int mkdir_with_parent(const std::string&);
bool is_arg(const char*, int, char*[]);
extern "C" void atexit_cleanup(void);
void clean_up_receive_dir(void);
bool is_ascii(const std::string&);
int is_in_process_table(pid_t);
char passp[50];
//char pass_str[50];
int main(int argc, char* argv[]) {

  // set up the locale for gettext() and base locale domain name of this program
#ifdef ENABLE_NLS
  bindtextdomain("efax-gtk", DATADIR "/locale");
  bind_textdomain_codeset("efax-gtk", "UTF-8");
  textdomain("efax-gtk");
  setlocale(LC_ALL,"");
#  ifdef HAVE_STREAM_IMBUE
  // make sure that the locale is set for C++ objects also
  // (this should also set it for C functions but calling setlocale()
  // explicitly above will also ensure POSIX compliance)
  try {
    std::locale::global(std::locale(""));
  }
  catch (std::exception& except) {
    const std::string message(except.what());
    write(2, message.data(), message.size());
  }
#  endif
#endif

  // provide a new process group with this process as the process group leader
  // so we can check whether another process is one of our children (ie a member
  // of our process group) and also call kill(0, SIGTERM) in atexit_cleanup()
  if (setpgid(getpid(), getpid()) == -1) {
    const std::string message("Error in creating new process group\n");
    write(2, message.data(), message.size());
  }

  if (argc > 1) {
    if (is_arg("--version", argc, argv)) {
      std::string message("efax-gtk-" VERSION "\n");
      message += gettext("Copyright (C) 2001 - 2007 Chris Vine\n"
			 "This program is released under the "
			 "GNU General Public License, version 2\n");
      try {
	message = Utf8::locale_from_utf8(message);
	write(1, message.data(), message.size());
      }
      catch (Utf8::ConversionError&) {
	std::string err_message("UTF-8 conversion error in main()\n");
	write(2, err_message.data(), err_message.size());
      }
      return 0;
    }
	
    if (is_arg("--help", argc, argv)) {
      std::string message("efax-gtk-" VERSION "\n");
      message += gettext("Usage: efax-gtk [options] [filename]\n"
			 "Options:\n"
			 "\t-r  Start the program in receive standby mode\n"
			 "\t-s  Start the program hidden in the system tray\n"
			 "See the README file which comes with the distribution\n"
			 "for further details\n");
      try {
	message = Utf8::locale_from_utf8(message);
	write(1, message.data(), message.size());
      }
      catch  (Utf8::ConversionError&) {
	std::string err_message("UTF-8 conversion error in main()\n");
	write(2, err_message.data(), err_message.size());
      }
      return 0;
    }
  }

  // although we use Thread::Thread and Thread::Mutex classes based on pthreads,
  // we need to call g_thread_init() in order to make glib thread safe
  // (we do not use the GTK global thread lock, so there is no need to
  // call gdk_threads_init())
  g_thread_init(0);
  prog_config.mutex_p = new Thread::Mutex;
  
  std::string messages(configure_prog(false));

  if(!is_program_running()) {

    gtk_init(&argc, &argv);
    g_atexit(atexit_cleanup);

    // initialise Notifier objects
    try {
      Notifier::init();
    }
    catch (PipeError& error) {
      std::cerr << error.error_message;
      std::exit(PIPE_ERROR);
    }

    get_fonts();

    get_window_icon();

    if (!prog_config.GPL_flag) {
      GplDialog gpl_dialog(24);
      int result = gpl_dialog.exec();
      if (result == GplDialog::accepted) {
	std::string file_name(prog_config.working_dir + "/" MAINWIN_SAVE_FILE);
	std::ofstream file(file_name.c_str(), std::ios::out);
	prog_config.GPL_flag = true;
      }
      else beep();
    }
    if (prog_config.GPL_flag) {

      bool start_in_standby = false;
      bool start_hidden = false;
      int opt_result;
      while ((opt_result = getopt(argc, argv, "rs-:")) != -1) {
	switch(opt_result) {
	case 'r':
	  start_in_standby = true;
	  break;
	case 's':
	  start_hidden = true;
	  break;
	case '-':
	  std::string message(argv[0]);
	  message += ": ";
	  message += gettext("Invalid option.  Options are:\n");
	  message += "  -r\n"
	             "  -s\n"
	             "  --help\n"
		     "  --version\n";
	  write(2, message.data(), message.size());
	  break;
	}
      }
      const char* filename = 0;
      if (optind < argc) filename = argv[optind];

      // this program cleans up for closure by ordinary user action, and
      // atexit_cleanup() in main.cpp caters for cases where the X session
      // has brought about the closure of this program.  It is also possible
      // that no clean-up at all will occur if the program is terminated on,
      // say, a power failure.  So call the clean_up_receive_dir() function
      // now to deal with that case.
      clean_up_receive_dir();

      try {
	MainWindow mainwindow(messages, start_hidden, start_in_standby, filename);

	// everything is set up
	// now enter the main program loop
	gtk_main();
      }
      catch (PipeError& error) {
	std::cerr << error.error_message;
	std::exit(PIPE_ERROR);
      }
    }
  }
#ifdef HAVE_NOTIFY_STARTUP
  else {
    // if we have got here then is_program_running() will have detected
    // that there is already an instance of this program running and
    // it will have sent a message to that instance via a fifo to present
    // itself on the current workspace.  We now need to tell the window
    // manager that we have completed startup, or the window manager
    // startup notifier will be confused
    // gdk_notify_startup_complete() is first available with GTK+-2.2
    gtk_init(&argc, &argv);
    gdk_notify_startup_complete();
  }
#endif
  delete prog_config.mutex_p;
  return 0;
}

std::string configure_prog(bool reread) {

  // lock the Prog_config object while we modify it
  Thread::Mutex::Lock lock(*prog_config.mutex_p);

  if (!reread) {

    char* home = std::getenv("HOME");

    if (!home) write_error("Your HOME environmental variable is not defined!\n");
    else {
      prog_config.homedir = home;
      prog_config.working_dir = home;
    }
  }

// now find rc file

  prog_config.found_rcfile = false;
  std::ifstream filein;
  std::string rcfile;
  std::string return_val;

  if (!prog_config.homedir.empty()) {
    rcfile = prog_config.homedir;
    rcfile += "/." RC_FILE;

#ifdef HAVE_IOS_NOCREATE
    filein.open(rcfile.c_str(), std::ios::in | std::ios::nocreate);
#else
    // we must have Std C++ so we probably don't need a ios::nocreate
    // flag on a read open to ensure uniqueness
    filein.open(rcfile.c_str(), std::ios::in);
#endif

    if (filein) prog_config.found_rcfile = true;
    else filein.clear();
  }

  if (!prog_config.found_rcfile) {

    rcfile = RC_DIR "/" RC_FILE;

#ifdef HAVE_IOS_NOCREATE
    filein.open(rcfile.c_str(), std::ios::in | std::ios::nocreate);
#else
    // we must have Std C++ so we probably don't need a ios::nocreate
    // flag on a read open to ensure uniqueness
    filein.open(rcfile.c_str(), std::ios::in);
#endif

    if (filein) prog_config.found_rcfile = true;
    else filein.clear();
  }

  if (!prog_config.found_rcfile && std::strcmp(RC_DIR, "/etc")) {

    rcfile = "/etc/" RC_FILE;

#ifdef HAVE_IOS_NOCREATE
    filein.open(rcfile.c_str(), std::ios::in | std::ios::nocreate);
#else
    // we must have Std C++ so we probably don't need a ios::nocreate
    // flag on a read open to ensure uniqueness
    filein.open(rcfile.c_str(), std::ios::in);
#endif

    if (filein) prog_config.found_rcfile = true;
    else filein.clear();
  }

  if (!prog_config.found_rcfile) {
    return_val = "Can't find or open file " RC_DIR "/"  RC_FILE ",\n";
    if (std::strcmp(RC_DIR, "/etc")) {
      return_val += "/etc/" RC_FILE ", ";
    }
    if (!prog_config.homedir.empty()) {
      try {
	std::string temp(Utf8::filename_to_utf8(prog_config.homedir));
	return_val +=  "or ";
	return_val += temp;
	return_val += "/." RC_FILE;
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in configure_prog()\n");
      }
    }
    return_val += "\n";
  }
  
  else {

    // if we are re-reading efax-gtkrc, we need to clear the old settings
    if (reread) {
      prog_config.lock_file = "";
      prog_config.my_name = "";
      prog_config.my_number = "";
      prog_config.parms.clear();
      prog_config.page_size = "";
      prog_config.page_dim = "";
      prog_config.resolution = "";
      prog_config.dial_prefix = "";
      prog_config.print_cmd = "";
      prog_config.print_shrink = "";
      prog_config.ps_view_cmd = "";
      prog_config.sock_server_port = "";
      prog_config.fax_received_prog = "";
      prog_config.logfile_name = "";
      prog_config.permitted_clients_list.clear();
    }
    // if we are setting up for first time, initialise prog_config.efax_controller_child_pid;
    else prog_config.efax_controller_child_pid = 0;

// now extract settings from file

    std::string file_read;
    std::string device;
    std::string lock_file;
    std::string modem_class;
    std::string rings;
    std::string dialmode;
    std::string init;
    std::string reset;
    std::string capabilities;
    std::string extra_parms;
    std::string gtkprint;
    std::string print_popup;
    std::string sock_server;
    std::string sock_popup;
    std::string sock_client_address;
    std::string sock_other_addresses;
    std::string fax_received_popup;
    std::string fax_received_exec;
    std::string addr_in_header;
    std::string work_subdir;
    
    while (std::getline(filein, file_read)) {

      if (!file_read.empty() && file_read[0] != '#') { // valid line to check
	// now check for other comment markers
	std::string::size_type pos = file_read.find_first_of('#');
	if (pos != std::string::npos) file_read.resize(pos); // truncate
	
	// look for "NAME:"
	if (get_prog_parm("NAME:", file_read, prog_config.my_name));
	
	// look for "NUMBER:"
	else if (get_prog_parm("NUMBER:", file_read, prog_config.my_number));
	
	// look for "DEVICE:"
	else if (get_prog_parm("DEVICE:", file_read, device));
	
	// look for "LOCK:"
	else if (get_prog_parm("LOCK:", file_read, lock_file,
			       Utf8::filename_to_utf8));

	// look for "CLASS:"
	else if (get_prog_parm("CLASS:", file_read, modem_class));

	// look for "PAGE:"
	else if (get_prog_parm("PAGE:", file_read, prog_config.page_size));

	// look for "RES:"
	else if (get_prog_parm("RES:", file_read, prog_config.resolution));
	
	// look for "DIAL_PREFIX:"
	else if (get_prog_parm("DIAL_PREFIX:", file_read, prog_config.dial_prefix));
	
	// look for "RINGS:"
	else if (get_prog_parm("RINGS:", file_read, rings));
	
	// look for "DIALMODE:"
	else if (get_prog_parm("DIALMODE:", file_read, dialmode));
	
	// look for "INIT:"
	else if (get_prog_parm("INIT:", file_read, init));
	
	// look for "RESET:"
	else if (get_prog_parm("RESET:", file_read, reset));

	// look for "CAPABILITIES:"
	else if (get_prog_parm("CAPABILITIES:", file_read, capabilities));

	// look for "PARMS:"
	else if (get_prog_parm("PARMS:", file_read, extra_parms));

	// look for "PRINT_CMD:"
	else if (get_prog_parm("PRINT_CMD:", file_read, prog_config.print_cmd));

	// look for "USE_GTKPRINT:"
	else if (get_prog_parm("USE_GTKPRINT:", file_read, gtkprint));

	// look for "PRINT_SHRINK:"
	else if (get_prog_parm("PRINT_SHRINK:", file_read, prog_config.print_shrink));

	// look for "PRINT_POPUP:"
	else if (get_prog_parm("PRINT_POPUP:", file_read, print_popup));

	// look for "PS_VIEWER:"
	else if (get_prog_parm("PS_VIEWER:", file_read, prog_config.ps_view_cmd));

	// look for "SOCK_SERVER:"
	else if (get_prog_parm("SOCK_SERVER:", file_read, sock_server));

	// look for "SOCK_POPUP:"
	else if (get_prog_parm("SOCK_POPUP:", file_read, sock_popup));

	// look for "SOCK_SERVER_PORT:"
	else if (get_prog_parm("SOCK_SERVER_PORT:", file_read, prog_config.sock_server_port));

	// look for "SOCK_CLIENT_ADDRESS:"
	else if (get_prog_parm("SOCK_CLIENT_ADDRESS:", file_read, sock_client_address));

	// look for "SOCK_OTHER_ADDRESSES:"
	else if (get_prog_parm("SOCK_OTHER_ADDRESSES:", file_read, sock_other_addresses));

	// look for "FAX_RECEIVED_POPUP:"
	else if (get_prog_parm("FAX_RECEIVED_POPUP:", file_read, fax_received_popup));

	// look for "FAX_RECEIVED_EXEC:"
	else if (get_prog_parm("FAX_RECEIVED_EXEC:", file_read, fax_received_exec));

	// look for "FAX_RECEIVED_PROG:"
	else if (get_prog_parm("FAX_RECEIVED_PROG:", file_read, prog_config.fax_received_prog,
			       Utf8::filename_to_utf8));

	// look for "LOG_FILE:"
	else if (get_prog_parm("LOG_FILE:", file_read, prog_config.logfile_name,
			       Utf8::filename_to_utf8));

	// look for "ADDR_IN_HEADER:"
	else if (get_prog_parm("ADDR_IN_HEADER:", file_read, addr_in_header));

	// look for "WORK_SUBDIR:"
	else if (get_prog_parm("WORK_SUBDIR:", file_read, work_subdir,
			       Utf8::filename_to_utf8));
      }
    }

    // we have finished reading the configuration file    
    // now enter parameters common to send and receive of faxes	
    prog_config.parms.push_back("efax-0.9a");

    prog_config.parms.push_back("-vew");  // stderr -- errors and warnings
    
    prog_config.parms.push_back("-vin");  // stdin -- information and negotiations
    //prog_config.parms.push_back("-vina");  // this will also report the parameters passed to efax
                                             // uncomment it and comment out preceding line for debugging

    prog_config.parms.push_back("-u");    // force use of UTF-8 to send messages to efax-gtk
    
    std::string temp;

    if (!is_ascii(prog_config.my_name)) {
      prog_config.my_name = "";
      return_val += gettext("Invalid user name specified - it must be in plain\n"
			    "ASCII characters.  No user name will be shown on\n"
			    "the fax top header line, but don't worry as the\n"
			    "fax station number will always be given on the\n"
			    "top header\n");
    }

    if (!prog_config.my_number.empty()) {
      temp = "-l";
      temp += prog_config.my_number;
      prog_config.parms.push_back(temp);
    }

    if (!init.empty()) {
      std::string::size_type start = 0;
      std::string::size_type end;
      
      while (start != std::string::npos) {
	temp = "-i";
	end = init.find_first_of(" \t", start);
	if (end != std::string::npos) {
	  temp.append(init, start, end - start);
	  start = init.find_first_not_of(" \t", end); // prepare for the next iteration
	}
	else {
	  temp.append(init, start, init.size() - start);
	  start = end;
	}
	prog_config.parms.push_back(temp);
      }
    }
    
    else {
      prog_config.parms.push_back("-iZ");
      prog_config.parms.push_back("-i&FE&D2S7=120");
      prog_config.parms.push_back("-i&C0");
      prog_config.parms.push_back("-iM1L0");
      
    }
    
    if (!reset.empty()) {
      std::string::size_type start = 0;
      std::string::size_type end;
    
      while (start != std::string::npos) {
	temp = "-k";
	end = reset.find_first_of(" \t", start);
	if (end != std::string::npos) {
	  temp.append(reset, start, end - start);
	  start = reset.find_first_not_of(" \t", end); // prepare for the next iteration
	}
	else {
	  temp.append(reset, start, reset.size() - start);
	  start = end;
	}
	prog_config.parms.push_back(temp);
      }
    }

    else prog_config.parms.push_back("-kZ");
    
    if (!modem_class.empty()) {
      temp = "-o";
      if (!modem_class.compare("2.0")) temp += '0';
      else if (!modem_class.compare("1")) temp += '1';
      else if (!modem_class.compare("2")) temp += '2';
      else {
	return_val += gettext("Invalid modem class specified\n"
			      "Adopting default of Class 2\n");
        temp += '2';
      }
      prog_config.parms.push_back(temp);
    }
      
    if (device.empty()) {
      if (access("/dev/modem", F_OK) == -1) {
	return_val += gettext("No serial port device specified in " RC_FILE " configuration file\n"
			      "and /dev/modem does not exist\n");
      }
      
      else {
	return_val += gettext("No serial port device specified in " RC_FILE " configuration file\n"
			      "Using default of /dev/modem\n");
	device = "modem";
      }
    }

    if (!device.empty()) {
      std::string locale_device;
      try {
	locale_device = Utf8::locale_from_utf8(device);
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in configure_prog() - device\n");
      }

      if (!locale_device.empty()) {
	if (lock_file.empty()) {
	  prog_config.lock_file = "/var/lock";
	}
	else {
	  try {
	    prog_config.lock_file = Utf8::filename_from_utf8(lock_file);
	  }
	  catch (Utf8::ConversionError&) {
	    write_error("UTF-8 conversion error in configure_prog() - lock file\n");
	    write_error("Defaulting to /var/lock\n");
	    prog_config.lock_file = "/var/lock";
	  }
	}
	
	prog_config.lock_file += "/LCK..";
	temp = device;
	// replace any '/' characters with '_' character
	std::replace(temp.begin(), temp.end(), '/', '_');
	prog_config.lock_file += temp;
	
	temp = "-d/dev/";
	temp += locale_device;
	prog_config.parms.push_back(temp);

	temp = "-x";
	temp += prog_config.lock_file;
	prog_config.parms.push_back(temp);
      }
    }

    if (!capabilities.empty()) {
      temp = "-c";
      temp += capabilities;
      prog_config.parms.push_back(temp);
    }

    if (prog_config.resolution.empty()) {
      return_val += gettext("Adopting default fax resolution of 204x196\n");
      prog_config.resolution = "204x196";
    }
    else {
      temp = Utf8::to_lower(prog_config.resolution);
      if (!temp.compare("fine")) prog_config.resolution = "204x196";
      else if (!temp.compare("standard")) prog_config.resolution = "204x98";
      else {
	return_val += gettext("Invalid fax resolution specified\n"
			      "Adopting default fax resolution of 204x196\n");
	prog_config.resolution = "204x196";
      }
    }
    
    if (rings.empty()) prog_config.rings = '1';
    else if (rings.size() > 1 || rings[0] < '1' || rings[0] > '9') {
      return_val += gettext("Invalid ring number specified\n"
			    "Will answer after one ring\n");
      prog_config.rings = '1';
    }
    else prog_config.rings = rings[0];

    if (prog_config.page_size.empty()) {
      return_val += gettext("Adopting default page size of a4\n");
      prog_config.page_size = "a4";
      prog_config.page_dim = "210x297mm";
    }
    else {
      prog_config.page_size = Utf8::to_lower(prog_config.page_size);
      if (!prog_config.page_size.compare("a4")) prog_config.page_dim = "210x297mm";
      else if (!prog_config.page_size.compare("letter")) prog_config.page_dim = "216x279mm";
      else if (!prog_config.page_size.compare("legal")) prog_config.page_dim = "216x356mm";
      else {
	return_val += gettext("Invalid page size specified\n"
			      "Adopting default page size of a4\n");
	prog_config.page_size = "a4";
	prog_config.page_dim = "210x297mm";
      }
    }
    
    if (dialmode.empty()) prog_config.tone_dial = true;
    else {
      temp = Utf8::to_lower(dialmode);
      if (!temp.compare("tone")) prog_config.tone_dial = true;
      else if (!temp.compare("pulse")) prog_config.tone_dial = false;
      else {
	return_val += gettext("Invalid dialmode specified\n"
			      "Adopting default of tone dialling\n");
	prog_config.tone_dial = true;
      }
    }

    if (!extra_parms.empty()) {
      std::string::size_type start = 0;
      std::string::size_type end;
    
      while (start != std::string::npos) {
	end = extra_parms.find_first_of(" \t", start);
	if (end != std::string::npos) {
	  temp.assign(extra_parms, start, end - start);
	  start = extra_parms.find_first_not_of(" \t", end); // prepare for the next iteration
	}
	else {
	  temp.assign(extra_parms, start, extra_parms.size() - start);
	  start = end;
	}
	prog_config.parms.push_back(temp);
      }
    }
    if (prog_config.print_cmd.empty()) {
      prog_config.print_cmd = "lpr";
    }
    
    temp = Utf8::to_lower(gtkprint);
#if !(GTK_CHECK_VERSION(2,10,0))
    temp = "no";
#endif
    if (!temp.compare("yes")) prog_config.gtkprint = true;
    else prog_config.gtkprint = false;

    if (prog_config.print_shrink.empty()) {
      prog_config.print_shrink = "100";
    }
    else if (std::atoi(prog_config.print_shrink.c_str()) < 50 || std::atoi(prog_config.print_shrink.c_str()) > 100) {
      return_val += gettext("Invalid print shrink specified: adopting default value of 100\n");
      prog_config.print_shrink = "100";
    }
    
    temp = Utf8::to_lower(print_popup);
    if (!temp.compare("no")) prog_config.print_popup = false;
    else prog_config.print_popup = true;

    if (prog_config.ps_view_cmd.empty()) {
      return_val += gettext("Adopting default postscript view command of 'gv'\n");
      prog_config.ps_view_cmd = "gv";
    }

    temp = Utf8::to_lower(sock_server);
    if (!temp.compare("yes")) prog_config.sock_server = true;
    else prog_config.sock_server = false;

    temp = Utf8::to_lower(sock_popup);
    if (!temp.compare("yes")) prog_config.sock_popup = true;
    else prog_config.sock_popup = false;

    if (prog_config.sock_server_port.empty()) {
      if (prog_config.sock_server) 
	return_val += gettext("No port for the socket server has been specified, "
			      "so the server will not be started\n");
    }

    else if (std::atoi(prog_config.sock_server_port.c_str()) < 1024
	     || std::atoi(prog_config.sock_server_port.c_str()) > 65535) {
      return_val += gettext("Invalid port for the socket server has been specified, "
			    "so the server will not be started.  It needs to be between "
			    "1024 and 65535\n");
      prog_config.sock_server_port = "";
    }

    temp = Utf8::to_lower(sock_client_address);
    if (!temp.compare("other")) prog_config.other_sock_client_address = true;
    else prog_config.other_sock_client_address = false;

    if (!sock_other_addresses.empty()) {
      std::string::size_type start = 0;
      std::string::size_type end;
    
      while (start != std::string::npos) {
	end = sock_other_addresses.find_first_of(" \t", start);
	if (end != std::string::npos) {
	  temp.assign(sock_other_addresses, start, end - start);
	  start = sock_other_addresses.find_first_not_of(" \t", end); // prepare for the next iteration
	}
	else {
	  temp.assign(sock_other_addresses, start, sock_other_addresses.size() - start);
	  start = end;
	}
	prog_config.permitted_clients_list.push_back(temp);
      }
    }

    temp = Utf8::to_lower(fax_received_popup);
    if (!temp.compare("yes")) prog_config.fax_received_popup = true;
    else prog_config.fax_received_popup = false;

    temp = Utf8::to_lower(fax_received_exec);
    if (!temp.compare("yes")) prog_config.fax_received_exec = true;
    else prog_config.fax_received_exec = false;

    temp = Utf8::to_lower(addr_in_header);
    if (!temp.compare("no")) prog_config.addr_in_header = false;
    else prog_config.addr_in_header = true;

    if (!reread) {  // prog_config.working_dir and prog_config.GPL_flag are not
                    // affected by the settings dialog and so not re-configurable
                    // after the program has initialised itself for the first time
      // CMV 24-07-04
      if (!work_subdir.empty()) {
	prog_config.working_dir += '/';
	prog_config.working_dir += Utf8::filename_from_utf8(work_subdir);
	if (mkdir_with_parent(prog_config.working_dir) && errno != EEXIST) {
	  return_val += gettext("Invalid WORK_SUBDIR: directory specified. "
				"WORK_SUBDIR: will be ignored\n");
	  prog_config.working_dir = prog_config.homedir;
	}
      }

      std::string file_name(prog_config.working_dir + "/" MAINWIN_SAVE_FILE);
      int result = access(file_name.c_str(), F_OK);
  
      if (!result) prog_config.GPL_flag = true;
      else prog_config.GPL_flag = false;
    }
  }
  return return_val;
}

void get_fonts(void) {

#ifdef HAVE_X11_XLIB_H
  // this will get a suitable fixed font for GplDialog and HelpDialog to use with Pango
  const int MAX_FONTS = 10000;
  int num_fonts;
  char** fonts = XListFonts(GDK_DISPLAY(), "-*", MAX_FONTS, &num_fonts);

  if (fonts) {
    int count;
    std::string inspect_name;
    prog_config.fixed_font = "";

    //try for courier font
    for (count = 0; count < num_fonts; count++) {
      inspect_name = fonts[count];
      std::string::size_type pos = inspect_name.find("courier-medium-r-normal-");
      if (pos != std::string::npos) {
	prog_config.fixed_font = "courier";
	break;
      }
    }

    // unsuccessful -- go for the generic "fixed" font
    if (prog_config.fixed_font.empty()) prog_config.fixed_font = "fixed";

    XFreeFontNames(fonts);
  }
  else prog_config.fixed_font = "fixed";
#else
  prog_config.fixed_font = "fixed";
#endif
}

bool get_prog_parm(const char* name, std::string& line, std::string& result,
		   std::string(*convert_func)(const std::string&)) {
// This function looks for a setting named `name' in the string `line'
// and returns the values stated after it in string `result'.  It returns
// `true' if the setting was found.  If there are trailing spaces or tabs,
// string `line' will be modified.  string `result' is only modified if
// the `name' setting is found.  Anything extracted from `line' will be
// converted (when placed into `result') to UTF-8, using the function
// assigned to function pointer convert_func (you would normally use
// Utf8::locale_to_utf8() or Utf8::filename_to_utf8(), and there is a
// default inline function using Utf8::locale_to_utf8())

  const std::string::size_type length = std::strlen(name);
  // we have to use std::string::substr() because libstdc++-2
  // doesn't support the Std-C++ std::string::compare() functions
  if (!line.substr(0, length).compare(name)) {
    // erase any trailing space or tab
    while (line.find_last_of(" \t") == line.size() - 1) line.resize(line.size() - 1);
    if (line.size() > length) {
      // ignore any preceding space or tab from the setting value given
      std::string::size_type pos = line.find_first_not_of(" \t", length); // pos now is set to beginning of setting value
      if (pos != std::string::npos) {
	try {
	  result.assign(convert_func(line.substr(pos)));
	}
	catch (Utf8::ConversionError&) {
	  result = "";
	  write_error("UTF-8 conversion error in get_prog_parm()\n");
	}
      }
    }
    return true;
  }
  return false;
}

void beep(void) {
#ifdef HAVE_X11_XLIB_H
  XBell(GDK_DISPLAY(), 0);
#else
  gdk_beep();
#endif
}

void get_window_icon(void) {

  bool have_icon = false;

#ifdef EFAX_GTK_USE_ICON_THEME

  GtkIconTheme* icon_theme_p = gtk_icon_theme_get_default();

  IconInfoScopedHandle icon_info_h(gtk_icon_theme_lookup_icon(icon_theme_p,
							      "stock_send-fax",
							      24, GtkIconLookupFlags(0)));
  if (icon_info_h.get()) {
      
    const gchar* icon_path_p = gtk_icon_info_get_filename(icon_info_h.get());
    if (icon_path_p) {
      GError* error_p = 0;
      prog_config.window_icon_h =
	GobjHandle<GdkPixbuf>(gdk_pixbuf_new_from_file(icon_path_p, &error_p));
      if (prog_config.window_icon_h.get()) have_icon = true;
      else {
	write_error("Pixbuf error in get_window_icon()\n");
	if (error_p) {
	  GerrorScopedHandle handle_h(error_p);
	  write_error(error_p->message);
	  write_error("\n");
	}
      }
    }
  }
#endif

  if (!have_icon) {
    prog_config.window_icon_h =
      GobjHandle<GdkPixbuf>(gdk_pixbuf_new_from_xpm_data(window_icon_xpm));
  }
}

int mkdir_with_parent(const std::string& dir) {

  // this function must be passed an absolute path name
  // on success it returns 0, and on failure it returns -1.  If -1 is returned
  // for any reason other than that an absolute pathname was not passed
  // then errno will be set by Unix mkdir()

  // function provided by CMV 24-07-04

  int return_val = 0;

  if (dir[0] != '/') return_val = -1;

  else {
    std::string::size_type pos = 0;
    while (pos != std::string::npos && (!return_val || errno == EEXIST)) {
      pos = dir.find('/', ++pos);
      if (pos != std::string::npos) {
	return_val = mkdir(dir.substr(0, pos).c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
      }
      else {
	return_val = mkdir(dir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
      }
    }
  }
  return return_val;
}

bool is_arg(const char* arg, int argc, char* argv[]) {

  bool return_val;
  int count;
  for (return_val = false, count = 1; !return_val && count < argc; count++) {
    if (!std::strcmp(argv[count], arg)) return_val = true;
  }
  return return_val;
}

bool is_program_running(void) {

  bool return_val = false;
  int count = 0;

  std::string lockfile(prog_config.working_dir);
  lockfile += LOCKFILE_NAME;

  // do an atomic exclusive write open on the lockfile
  // we do not need to check for EINTR on the open as we do not catch
  // any signals at this stage
  int new_lock_fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  while (count < 2 && new_lock_fd == -1) {      // check to see why we could not open new_lock
    if (access(lockfile.c_str(), F_OK) == -1) { // file doesn't exist, so there must be a problem with permissions

      std::cerr << "Cannot create lockfile " << lockfile << std::endl;
    }

    else {   // the lockfile already exists - see if it is stale
             // it shouldn't ever be stale with the other clean up provision
             // in this program, but nonetheless ...
      std::ifstream existing_lock(lockfile.c_str(), std::ios::in);
      if(!existing_lock) { // can't open lock file
	std::cerr << "Not able to access existing lockfile " << lockfile << std::endl;
	// get rid of the old lockfile if we can
	unlink(lockfile.c_str());
      }
      else { // lock file already exists and we can open it - check if stale

#ifdef HAVE_STREAM_IMBUE
	existing_lock.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

	// test with kill() as well as with is_in_process_table()
	// in case SUS 'ps' arguments are not available
	pid_t existing_lock_pid;
	existing_lock >> existing_lock_pid;
	if (!is_in_process_table(existing_lock_pid)
	    || (existing_lock && kill(existing_lock_pid, 0) < 0)) { // stale lock
	  unlink(lockfile.c_str());    // delete stale lock file
	  new_lock_fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL,
			     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	  count++;                     // signal we can only have one more try if this open fails
	}
	else {                         // active lock - alert the program pipe
	  std::cout << "Active lock - bringing up existing instance of efax-gtk" << std::endl;
	  count = 2;                   // break out of the loop

	  std::string fifo_name(prog_config.working_dir);
	  fifo_name += PIPE_NAME;
	  
	  if (!access(fifo_name.c_str(), F_OK)) { // if we don't enter this block we have a
                                                  // problem - the fifo should exist but doesn't
	    int write_fd = open(fifo_name.c_str(), O_WRONLY | O_NONBLOCK);
	    if (write_fd >= 0) {                  // if we don't enter this block we also have a
                                                  // problem - the fifo can't be opened for writing
	      Attachable::Out pipe_stream;

#ifdef HAVE_STREAM_IMBUE
	      pipe_stream.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

	      pipe_stream.attach(write_fd);
	      pipe_stream << existing_lock_pid << std::endl;
	      pipe_stream.close();
	      return_val = true;
	    }
	    else std::cerr << "Error: cannot open fifo for writing" << std::endl;
	  }
	  else std::cerr << "Error: program fifo doesn't exist" << std::endl;
	}
      }
    }
  }
  if (count < 2 && new_lock_fd >= 0)  {// we have successfully created a lock file - now write our pid to it

    Attachable::Out new_lock;

#ifdef HAVE_STREAM_IMBUE
    new_lock.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

    new_lock.attach(new_lock_fd);
    new_lock << std::setfill('0') << std::setw(10) << getpid() << std::endl;
    new_lock.close();
  }
  return return_val;
}

bool is_ascii(const std::string& text) {
  
  std::string::const_iterator end_iter = text.end();
  if (std::find_if(text.begin(), end_iter, not_ascii_char) == end_iter) {
    return true;
  }
  return false;
}

// returns 1 if efax-gtk is definitely in the process table for the PID in existing_lock_pid
// returns 0 if efax-gtk is definitely not in process table for the PID in existing_lock_pid
// returns -1 if we cannot tell because 'ps' fails (say because SUS ps arguments are not available)
int is_in_process_table(pid_t existing_lock_pid) {

  // call ourselves recursively to see whether we can find out own process id in the process table
  static bool checked = false;
  if (!checked) {
    checked = true;
    if (is_in_process_table(getpid()) != 1) {
      const std::string message("'ps' command is not POSIX conforming or is not in PATH\n");
      write(2, message.data(), message.size());
      return -1;
    }
  }

  PipeFifo fork_pipe(PipeFifo::block);
  // now fork to create the process which send the output of 'ps' to stdout
  pid_t pid = fork();

  if (pid == -1) {
    const std::string message("Fork error - exiting\n");
    write(2, message.data(), message.size());
    std::exit(FORK_ERROR);
  }
  
  if (!pid) { // child process which will send the output of 'ps' to stdout
  
    // this is the child process to write to stdout
    fork_pipe.connect_to_stdout();

    // the program is not multi-threaded at this point so there is no problem
    // with using non async-signal-safe functions before we exec()
#ifdef HAVE_STRINGSTREAM
    std::ostringstream strm;
#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif
    strm << "-p" << existing_lock_pid;
    execlp("ps", "ps", strm.str().c_str(), static_cast<char*>(0));
#else
    std::ostrstream strm;
#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif
    strm << "-p" << existing_lock_pid << std::ends;
    execlp("ps", "ps", strm.str(), static_cast<char*>(0));
#endif

    // if we reached this point, then the execlp() call must have failed
    _exit(EXIT_SUCCESS - 1);
  }

  // read from the pipe
  fork_pipe.make_readonly();
  ssize_t result;
  char buffer[PIPE_BUF];
  std::string output;
  while ((result = fork_pipe.read(buffer, PIPE_BUF)) > 0) {
    output.append(buffer, result);
  }

  // for debugging
  //write(1, output.data(), output.size());

  // we can safely use wait() here because we are setting the program up
  // and nothing else is calling wait() or waitpid() and we do not need
  // to worry about EINTR because there are no signals at this stage
  int status;
  if (output.empty() || wait(&status) == -1) return -1;
  int exit_code = EXIT_SUCCESS - 1;
  if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
  if (exit_code != EXIT_SUCCESS) return -1;

  if (output.find("efax-gtk") != std::string::npos) {
    return 1;
  }
  return 0;
}

void atexit_cleanup(void) {

  // this does any essential clean up, which is always required to make the program
  // restart correctly.  It is mainly relevant to a case where the program has closed
  // down because X server connection has closed - where the program is closed by direct
  // user input to the program, then the program generally cleans up elsewhere

  // check we are the process group leader (we should be as setpgid() was
  // called in main()) and if so kill any stray children, such as an efax
  // instance or a fax viewing instance
  if (getpgid(0) == getpid()) {

    // block the signal to us
    struct sigaction sig_act;
    sig_act.sa_handler = SIG_IGN;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;
    sigaction(SIGTERM, &sig_act, 0);

    // kill
    kill(0, SIGTERM);
  }
  else std::cerr << "Error: we are not a process group leader!" << std::endl;

  // remove program lock file
  std::string lockfile(prog_config.working_dir);
  lockfile += LOCKFILE_NAME;
  unlink(lockfile.c_str());

  // remove program fifo
  std::string fifo_name(prog_config.working_dir);
  fifo_name += PIPE_NAME;
  unlink(fifo_name.c_str());

  // clean up the receive_directory if necessary
  clean_up_receive_dir();

  // call up a shell to dispense with any stray viewing or printing files
  // (these may exist if the program crashes, or is exited when a fax is still
  // being viewed) whether this executes or not is pretty optional so whilst
  // I would normally avoid calling system(), it is acceptable in this limited
  // context (IMO)
  std::string command_line("rm -f ");
  command_line += prog_config.working_dir;
  command_line += "/efax-gtk-view-* ";
  command_line += prog_config.working_dir;
  command_line += "/efax-gtk-print-*";

  std::system(command_line.c_str());
}

void clean_up_receive_dir(void) {

  if (!prog_config.working_dir.empty()) {
    std::string full_dirname(prog_config.working_dir);
    full_dirname += "/faxin/current";
    int result = rmdir(full_dirname.c_str());
    if (result == -1 && errno == ENOTEMPTY) { // we must be in the middle of receiving a fax
                                              // when the program ended - save what we can

      std::cerr << "The program closed when receiving a fax\n"
	           "Attempting to save what we can"
		<< std::endl;

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
	std::cerr << "Can't create directory to save received fax on clean up.\n"
	             "This fax will not be saved!"
		  << std::endl;
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
	  std::cerr << "Can't open received fax directory.\n"
	               "This fax will not be saved!"
		    << std::endl;
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
	    std::cerr << "Cannot save all of the received fax.\n"
	                 "All or part of the fax will be missing!"
		      << std::endl;
	  }
	  else {
	    for (iter = filelist.begin(); iter != filelist.end(); ++iter) {
	      old_filename = old_dirname;
	      old_filename += '/';
	      old_filename += *iter;
	      unlink(old_filename.c_str());
	    }
	    if (rmdir(old_dirname.c_str())) {
	      std::cerr << "Can't delete directory " << old_dirname
			<< "\nThe contents should have been moved to "
			<< new_dirname
			<< "\nand it should now be empty"
			<< std::endl;
	    }
	  }	
	}

	// reset current directory
	std::string temp(prog_config.working_dir + "/faxin");
	chdir(temp.c_str());
      }
    }
  }
}
