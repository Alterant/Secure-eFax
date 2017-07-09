/* Copyright (C) 1999 - 2004 Chris Vine

The library comprised in this file or of which this file is part is
distributed by Chris Vine under the GNU Lesser General Public
License as follows:

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License, version 2.1, for more details.

   You should have received a copy of the GNU Lesser General Public
   License, version 2.1, along with this library (see the file LGPL.TXT
   which came with this source code package in the src/utils sub-directory);
   if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA, 02111-1307, USA.

*/

#ifndef PIPES_H
#define PIPES_H

#include "prog_defs.h"     // for ssize_t

#include <unistd.h>

#include <string>

/*

This class provides a simplified front end to unix write() and read()
of fifo pipes.  The constructor of the class may be passed an
enumerator to indicate whether the pipe is to read in non-blocking
mode.

With the write(char*, int) method, taking an array and int arguments,
the value of int (indicating the number of chars in the array to be
written) should usually be less than PIPE_BUF, as POSIX write()
guarantees that if it is less than PIPE_BUF, either all will be
written or, if the pipe is too full, none will be written.  This
prevents data interleaving if a number of independent writes to the
fifo are made.  The same is true of the write (char*) method taking
a string - if the string is longer than PIPE_BUF in size, data
interleaving may occur.  

The pipe is set to read in non-blocking mode if the constructor or
PipeFifo is passed the value PipeFifo::non_block.  If constructed
in this way, the pipe can also be set to write in non-block mode (eg 
to minimise the impact on another program running in a child process
to which the child process has exec()ed when monitoring its stdin
or stderr) by calling make_write_non_block().  However use this very
sparingly -- when set in this way write() will always try to write
something.  The atomic guarantees mentioned in the preceding paragraph
do not apply and data can be interleaved or lost.

PIPE_BUF is defined in limits.h, and is at least 512 bytes, and usually
4096 bytes, but may be calculated from other files included by limits.h.

Where a pipe is used to communicate between parent and child after a
call to fork(), the pipe will normally be used unidirectionally unless
guards or semaphores are used to prevent a process reading data
intended for the other.  However, because each process inherits its
own duplicate of the file descriptors, this cannot be enforced without
closing the read or write file descriptor for the process for which
the reading or writing is to be prohibited.  This can be done by the
process concerned calling the methods PipeFifo::make_writeonly() or
PipeFifo::make_readonly() after the fork.  If an attempt is made to
read or write to a descriptor closed in this way, the
PipeFifo::read() or PipeFifo::write() method will ensure that no
read or write will take place, and instead -2 will be returned.

The methods PipeFifo::connect_to_stdin(), PipeFifo::connect_to_stdout()
and PipeFifo::connect_to_stderr() are available to be used in the child
process before it exec()s another program so as to connect the pipe to
that program's stdin, stdout or stderr.  PipeFifo::connect_to_stdin()
cannot be used by a process after that process has called
PipeFifo::make_writeonly(), and PipeFifo::connect_to_stdout() and
PipeFifo::connect_to_stderr() cannot be used after the process has
called PipeFifo::make_readonly().  If that is attempted the methods
will return -1; otherwise they will return 0.  Furthermore, they
should only be used after the process creating the pipe has forked.
If the connection to stdin, stdout or stderr is to be made before the
fork, this must be done by hand using dup2(), PipeFifo::get_write_fd()/
PipeFifo::get_read_fd() and PipeFifo::make_read_only()/
PipeFifo::make_write_only().

If PipeFifo::connect_to_stdin() is called by a method,
PipeFifo::make_readonly() will also be called, and if
PipeFifo::connect_to_stdout() or PipeFifo::connect_to_stderr() are
called, PipeFifo::make_writeonly() will also be called.  This will 
isolate the use of the pipe by the child process to stdin, stdout or
stderr, as appropriate.

It uses no static members, so is thread safe as between different objects,
but its methods are not thread safe as regards any one object in the sense
that the read() and write() methods check the value of read_fd and write_fd
respectively (and get_read_fd() and get_write_fd() return them), and
make_writeonly(), make_readonly(), close(), connect_to_stdin(), open(),
connect_to_stdout() and connect_to_stderr() change those values.  Likewise
the read() and write() methods access read_blocking_mode and
write_blocking_mode respectively, and these are changed by open() and
make_write_non_block().  Provided there is no concurrent use of read(),
write(), get_read_fd() or get_write_fd() in one thread with a call of a
method which changes read_fd, write_fd, read_blocking_mode or
write_blocking_mode as described above in another thread then no mutex
is required to ensure thread safety.

All the read() and write() methods check for an interruption of the system
call from a signal (EINTR is checked), and will continue to read() or write()
where necessary.  Users do not need to check EINTR themselves.  Where the
write file descriptor is flagged as blocking (that is, where
PipeFifo::make_write_non_block() has not been called), then in the absence
of some other error, everything passed to PipeFifo::write() will be written
(but as mentioned above there may be data interleaving if this is greated
than PIPE_BUF in size).  Where the write file descriptor is flagged as
non-blocking, then the result of Unix write is returned, and less bytes than
those passed to PipeFifo::write() may have been written, or -1 may be
returned with errno set to EAGAIN - it is for the user to check this.

*/

struct PipeError {
  std::string error_message;
  PipeError(const char* msg): error_message(msg) {}
};

class PipeFifo {
public:
  enum Fifo_mode{block, non_block};
private:
  int read_fd;
  int write_fd;
  Fifo_mode read_blocking_mode;
  Fifo_mode write_blocking_mode;

  // this class cannot be copied - file descriptors are owned and not shared
  PipeFifo(const PipeFifo&);
  PipeFifo& operator=(const PipeFifo&);
public:
  // open() throws PipeError if opening the pipe fails
  void open(Fifo_mode);
  void close(void);
  ssize_t read(char*, size_t);        // returns -2 if read file descriptor invalid, and
                                      // otherwise it returns the result of unix read()

  int read(void);                     // returns -2 if read file descriptor invalid, 0 or -1
                                      // if unix read() returns either of those, and otherwise
                                      // returns the char at the front of the pipe

  ssize_t write(const char*);         // returns -2 if write file descriptor invalid, and
                                      // otherwise it returns the number of bytes written
                                      // or -1 if there has been an error (check errno to
                                      // see what it was). If the write file descriptor is
                                      // blocking then it will carry on blocking until
                                      // everything is sent, even if it exceeds PIPE_BUF
                                      // in size.  The argument must be null terminated

  ssize_t write(const char*, size_t); // returns -2 if write file descriptor invalid, and
                                      // otherwise it returns the number of bytes written
                                      // or -1 if there has been an error (check errno to
                                      // see what it was). If the write file descriptor is
                                      // blocking then it will carry on blocking until
                                      // everything is sent, even if it exceeds PIPE_BUF
                                      // in size

  int write(char item) {return write(&item, 1);}
                                      // returns -2 if write file descriptor invalid, 1 if
                                      // char written, or -1 on error.  You can check
                                      // errno to see what the error was

  void make_writeonly(void);
  void make_readonly(void);
  int make_write_non_block(void);
  int get_read_fd(void) const {return read_fd;}
  int get_write_fd(void) const {return write_fd;}
  int connect_to_stdin(void);
  int connect_to_stdout(void);
  int connect_to_stderr(void);

  // the constructor taking a Fifo_mode argument throws PipeError
  // if opening the pipe fails
  PipeFifo(Fifo_mode);

  // the default constructor does not throw
  PipeFifo(void);
  ~PipeFifo(void) {close();}
};


// this class enables synchronisation between processes after fork()ing
// the process to wait on the other one calls wait() at the point
// where it wishes to wait, and the other process calls release()
// when it wants to enable the other process to continue
// it is one-shot only - once it has released, it cannot re-block again
class SyncPipe {
  PipeFifo pipe_fifo;
public:
  void release(void) {pipe_fifo.make_writeonly(); pipe_fifo.make_readonly();}
  void wait(void);
  // the constructor throws PipeError if opening the pipe fails
  SyncPipe(void): pipe_fifo(PipeFifo::block) {}
};

#endif
