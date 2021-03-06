/*
 * Copyright 1997-2007 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *  
 */
// This file is a derivative work resulting from (and including) modifications
// made by Azul Systems, Inc.  The date of such changes is 2010.
// Copyright 2010 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, Inc., 1600 Plymouth Street, Mountain View, 
// CA 94043 USA, or visit www.azulsystems.com if you need additional information 
// or have any questions.


#include "allocation.hpp"
#include "arguments.hpp"
#include "defaultStream.hpp"
#include "mutexLocker.hpp"
#include "nativeInst_pd.hpp"
#include "ostream.hpp"
#include "tickProfiler.hpp"
#include "vm_version_pd.hpp"
#include "xmlstream.hpp"

#include "allocation.inline.hpp"
#include "atomic_os_pd.inline.hpp"
#include "mutex.inline.hpp"
#include "oop.inline.hpp"
#include "os_os.inline.hpp"
#include "thread_os.inline.hpp"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef AZ_PROFILER
#include <azprof/azprof_util.hpp>
#endif // AZ_PROFILER


extern "C" void jio_print(const char* s); // Declarationtion of jvm method

outputStream::outputStream(int width) {
  _width       = width;
  _position    = 0;
  _newlines    = 0;
  _precount    = 0;
  _indentation = 0;
}

outputStream::outputStream(int width, bool has_time_stamps) {
  _width       = width;
  _position    = 0;
  _newlines    = 0;
  _precount    = 0;
  _indentation = 0;
  if (has_time_stamps)  _stamp.update();
}

void outputStream::update_position(const char* s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char ch = s[i];
    if (ch == '\n') {
      _newlines += 1;
      _precount += _position + 1;
      _position = 0;
    } else if (ch == '\t') {
      _position += 8;
      _precount -= 7;  // invariant:  _precount + _position == total count
    } else {
      _position += 1;
    }
  }
}

// Execute a vsprintf, using the given buffer if necessary.
// Return a pointer to the formatted string.
const char* outputStream::do_vsnprintf(char* buffer, size_t buflen,
                                       const char* format, va_list ap,
                                       bool add_cr,
                                       size_t& result_len) {
  const char* result;
  if (add_cr)  buflen--;
  if (!strchr(format, '%')) {
    // constant format string
    result = format;
    result_len = strlen(result);
    if (add_cr && result_len >= buflen)  result_len = buflen-1;  // truncate
  } else if (format[0] == '%' && format[1] == 's' && format[2] == '\0') {
    // trivial copy-through format string
    result = va_arg(ap, const char*);
    result_len = strlen(result);
    if (add_cr && result_len >= buflen)  result_len = buflen-1;  // truncate
  } else if (vsnprintf(buffer, buflen, format, ap) >= 0) {
    result = buffer;
    result_len = strlen(result);
  } else {
    DEBUG_ONLY(warning("increase O_BUFLEN in ostream.hpp -- output truncated");)
    result = buffer;
    result_len = buflen - 1;
    buffer[result_len] = 0;
  }
  if (add_cr) {
    if (result != buffer) {
      strncpy(buffer, result, buflen);
      result = buffer;
    }
    buffer[result_len++] = '\n';
    buffer[result_len] = 0;
  }
  return result;
}

void outputStream::print(const char* format, ...) {
  char buffer[O_BUFLEN];
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, ap, false, len);
  write(str, len);
  va_end(ap);
}

void outputStream::print_cr(const char* format, ...) { 
  char buffer[O_BUFLEN];
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, ap, true, len);
  write(str, len);
  va_end(ap);
}

void outputStream::vprint(const char *format, va_list argptr) { 
  char buffer[O_BUFLEN];
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, argptr, false, len);
  write(str, len);
}

void outputStream::vprint_cr(const char* format, va_list argptr) {
  char buffer[O_BUFLEN];
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, argptr, true, len);
  write(str, len);
}

void outputStream::fill_to(int col) {
  int dbg_count = 0;
  while (position() < col) {
    assert(dbg_count <= col, "endless loop!");
    dbg_count++;
    sp();
  }
}

void outputStream::put(char ch) {
  assert(ch != 0, "please fix call site");
  char buf[] = { ch, '\0' };
  write(buf, 1);
}

void outputStream::sp() {
  this->write(" ", 1);
}

void outputStream::cr() {
  this->write("\n", 1);
}

void outputStream::stamp() {
  if (! _stamp.is_updated()) {
    _stamp.update(); // start at 0 on first call to stamp()
  }

  // outputStream::stamp() may get called by ostream_abort(), use snprintf
  // to avoid allocating large stack buffer in print().
  char buf[40];
  jio_snprintf(buf, sizeof(buf), "%.3f", _stamp.seconds());
  print_raw(buf);
}

void outputStream::date_stamp(bool guard,
                              const char* prefix,
                              const char* suffix) {
  if (!guard) {
    return;
  }
  print_raw(prefix);
  static const char error_time[] = "yyyy-mm-ddThh:mm:ss.mmm+zzzz";
  static const int buffer_length = 32;
  char buffer[buffer_length];
  const char* iso8601_result = os::iso8601_time(buffer, buffer_length);
  if (iso8601_result != NULL) {
    print_raw(buffer);
  } else {
    print_raw(error_time);
  }
  print_raw(suffix);
  return;
}

void outputStream::indent() {
  while (_position < _indentation) sp();
}

void outputStream::print_jlong(jlong value) {
  // N.B. Same as INT64_FORMAT
  print(os::jlong_format_specifier(), value);
}

void outputStream::print_julong(julong value) {
  // N.B. Same as UINT64_FORMAT
  print(os::julong_format_specifier(), value);
}

stringStream::stringStream(const stringStream* ss) : outputStream() {
  init(ss->_resource, ss->buffer_length);

  strncpy(buffer, ss->buffer, ss->buffer_length); 
}

stringStream::stringStream(bool resource, size_t initial_size) : outputStream() {
  init(resource, initial_size);
}

// useful for output to fixed chunks of memory, such as performance counters
void stringStream::init(bool resource, size_t initial_size) {
  _resource     = resource;
  buffer_fixed  = false;
  if (_resource) {
    buffer_length = initial_size;
    buffer        = NEW_RESOURCE_ARRAY(char, buffer_length);
    buffer_pos    = 0;  
  } else {
    buffer_length = initial_size;
    buffer        = NEW_C_HEAP_ARRAY(char, buffer_length);
    buffer_pos    = 0;  
  }
}

stringStream::stringStream(char* fixed_buffer, size_t fixed_buffer_size) : outputStream() {
  _resource     = false;
  buffer_length = fixed_buffer_size;
  buffer        = fixed_buffer;
  buffer_pos    = 0;  
  buffer_fixed  = true;
}

void stringStream::write(const char* s, size_t len) {
  debug_only( if( _resource ) _nesting.check(); );
  size_t write_len = len;               // number of non-null bytes to write
  size_t end = buffer_pos + len + 1;    // position after write and final '\0'
  if (end >= buffer_length) {
    if (!_resource) {
      if (!buffer_fixed) {
        // For small overruns, double the buffer.  For larger ones, 
        // increase to the requested size.
        if (end < buffer_length * 2) {
          end = buffer_length * 2;
        }
        char* oldbuf = buffer;
buffer=NEW_C_HEAP_ARRAY(char,end);
        strncpy(buffer, oldbuf, buffer_pos);
        buffer_length = end;
FREE_C_HEAP_ARRAY(char,oldbuf);
      } else {
        // if buffer cannot resize, silently truncate
        end = buffer_length;
        write_len = end - buffer_pos - 1; // leave room for the final '\0'
      }
    } else {
      // For small overruns, double the buffer.  For larger ones, 
      // increase to the requested size.
      if (end < buffer_length * 2) {
        end = buffer_length * 2;
      }
      char* oldbuf = buffer;
      buffer = NEW_RESOURCE_ARRAY(char, end);
      strncpy(buffer, oldbuf, buffer_pos);
      buffer_length = end;
    }
  }
  // invariant: buffer is always null-terminated
  guarantee(buffer_pos + write_len + 1 <= buffer_length, "stringStream oob");
  buffer[buffer_pos + write_len] = 0;
  strncpy(buffer + buffer_pos, s, write_len);
  buffer_pos += write_len;
  // Note that the following does not depend on write_len.
  // This means that position and count get updated
  // even when overflow occurs.
  update_position(s, len);
}

size_t stringStream::size_to_fit() {
  if (!_resource && !buffer_fixed) {
    char* oldbuf = buffer;
    buffer = NEW_C_HEAP_ARRAY(char, buffer_pos+1);
strncpy(buffer,oldbuf,buffer_pos);
buffer[buffer_pos]='\0';
FREE_C_HEAP_ARRAY(char,oldbuf);
  }
  return buffer_pos+1;
}

char* stringStream::as_string() {
  char* copy = NEW_RESOURCE_ARRAY(char, buffer_pos+1);
  strncpy(copy, buffer, buffer_pos);
  copy[buffer_pos] = 0;  // terminating null
  return copy;
}

stringStream::~stringStream() {
  if (!_resource && !buffer_fixed) {
    FREE_C_HEAP_ARRAY(char, buffer);
  }
}

xmlStream*   xtty;
outputStream* tty;
gclogStream*  gclog;
#ifdef AZ_PROFILER
azprof::IORingBuffer* gc_buf = NULL;
#endif // AZ_PROFILER
outputStream* gclog_or_tty;
outputStream*cpilog_or_tty;

gclogStream::gclogStream() {
  _file = -1;
_file_name=NULL;
  _append = false;
}

gclogStream::~gclogStream() {
  close();
}

void gclogStream::flush(){
  ::fsync(_file);
}

void gclogStream::write(const char*s,size_t len){
  ttyLocker ttyl;
  if (is_open()) {
    ::write(_file, s, len);
  }
#ifdef AZ_PROFILER
if(gc_buf!=NULL){
    gc_buf->write_fully(s, len);
  }
#endif // AZ_PROFILER
  update_position(s, len);
}

void gclogStream::open(const char* file_name, bool append) {
  assert0(!is_open());
  ttyLocker ttyl;
  _file = ::open(file_name, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC),
                 S_IREAD|S_IWRITE|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  if (_file >= 0) {
int len=strlen(file_name);
_file_name=NEW_C_HEAP_ARRAY(char,len+1);
    memcpy(_file_name, file_name, len);
_file_name[len]='\0';
    _append = append;
  }
}

void gclogStream::close(){
  ttyLocker ttyl;
  if (is_open()) {
::close(_file);
    _file = -1;
FREE_C_HEAP_ARRAY(char,_file_name);
_file_name=NULL;
    _append = false;
  }
}

fileStream::fileStream(const char* file_name, bool append) {
  _file = ::open(file_name, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC),
                 S_IREAD|S_IWRITE|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  _need_close = true;
}

void fileStream::write(const char* s, size_t len) {
  if (is_open()) {
    ::write(_file, s, len);
  }
  update_position(s, len);
}

fileStream::~fileStream() {
  if (is_open()) {
    if (_need_close) ::close(_file);
    _file = -1;
  }
}

void fileStream::flush() {
  //::fsync(_file);
}

fdStream::fdStream(const char* file_name) {
  _fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  _need_close = true;
}

fdStream::~fdStream() {
  if (_fd != -1) {
    if (_need_close) close(_fd);
    _fd = -1;
  }
}

void fdStream::write(const char* s, size_t len) {
  if (_fd != -1) ::write(_fd, s, (int)len);
  update_position(s, len);
}

defaultStream* defaultStream::instance = NULL;
int defaultStream::_output_fd = 1;
int defaultStream::_error_fd  = 2;
FILE* defaultStream::_output_stream = stdout;
FILE* defaultStream::_error_stream  = stderr;

#define LOG_MAJOR_VERSION 160
#define LOG_MINOR_VERSION 1

void defaultStream::init() {
  _inited = true;
if(LogVMOutput){
    init_log();
  }
}

bool defaultStream::has_log_file() { 
  // lazily create log file (at startup, LogVMOutput is false even
  // if +LogVMOutput is used, because the flags haven't been parsed yet)
  // For safer printing during fatal error handling, do not init logfile
  // if a VM error has been reported.
  if (!_inited && !is_error_reported())  init();
  return _log_file != NULL; 
}

static const char* make_log_name(const char* log_name, const char* force_directory, char* buf) {
  const char* basename = log_name;
  char file_sep = os::file_separator()[0];
  const char* cp;
  for (cp = log_name; *cp != '\0'; cp++) {
    if (*cp == '/' || *cp == file_sep) {
      basename = cp+1;
    }
  }
  const char* nametail = log_name;

  strcpy(buf, "");
  if (force_directory != NULL) {
    strcat(buf, force_directory);
    strcat(buf, os::file_separator());
    nametail = basename;       // completely skip directory prefix
  }

  const char* star = strchr(basename, '*');
  int star_pos = (star == NULL) ? -1 : (star - nametail);

  if (star_pos >= 0) {
    // convert foo*bar.log to foo123bar.log
    int buf_pos = (int) strlen(buf);
    strncpy(&buf[buf_pos], nametail, star_pos);
    sprintf(&buf[buf_pos + star_pos], "%u", os::current_process_id());
    nametail += star_pos + 1;  // skip prefix and star
  }

  strcat(buf, nametail);      // append rest of name, or all of name
  return buf;
}

void defaultStream::init_log() {
  // %%% Need a MutexLocker?
const char*log_name=((LogFile!=NULL)&&(strlen(LogFile)>0))?LogFile:"hotspot.log";
  char buf[O_BUFLEN*2];
  const char* try_name = make_log_name(log_name, NULL, buf);
  fileStream* file = new(ResourceObj::C_HEAP) fileStream(try_name);
  if (!file->is_open()) {
    // Try again to open the file.
    char warnbuf[O_BUFLEN*2];
    sprintf(warnbuf, "Warning:  Cannot open log file: %s\n", try_name);
    // Note:  This feature is for maintainer use only.  No need for L10N.
    jio_print(warnbuf);
    try_name = make_log_name("hs_pid*.log", os::get_temp_directory(), buf);
    sprintf(warnbuf, "Warning:  Forcing option -XX:LogFile=%s\n", try_name);
    jio_print(warnbuf);
    delete file;
    file = new(ResourceObj::C_HEAP) fileStream(try_name);
  }
  if (file->is_open()) {
    _log_file = file;
    xmlStream* xs = new(ResourceObj::C_HEAP) xmlStream(file);
    _outer_xmlStream = xs;
    if (this == tty)  xtty = xs;
    // Write XML header.
    xs->print_cr("<?xml version='1.0' encoding='UTF-8'?>");
    // (For now, don't bother to issue a DTD for this private format.)
    jlong time_ms = os::javaTimeMillis() - tty->time_stamp().milliseconds();
    // %%% Should be: jlong time_ms = os::start_time_milliseconds(), if
    // we ever get round to introduce that method on the os class
    xs->head("hotspot_log version='%d %d'"
             " process='%d' time_ms='"INT64_FORMAT"'",
             LOG_MAJOR_VERSION, LOG_MINOR_VERSION,
             os::current_process_id(), time_ms);
    // Write VM version header immediately.
    xs->head("vm_version");
    xs->head("name"); xs->text("%s", VM_Version::vm_name()); xs->cr();
    xs->tail("name");
    xs->head("release"); xs->text("%s", VM_Version::vm_release()); xs->cr();
    xs->tail("release");
    xs->head("info"); xs->text("%s", VM_Version::internal_vm_info_string()); xs->cr();
    xs->tail("info");
    xs->tail("vm_version");
    // Record information about the command-line invocation.
    xs->head("vm_arguments");  // Cf. Arguments::print_on()
    if (Arguments::num_jvm_flags() > 0) {
      xs->head("flags");
      Arguments::print_jvm_flags_on(xs->text());
      xs->tail("flags");
    }
    if (Arguments::num_jvm_args() > 0) {
      xs->head("args");
      Arguments::print_jvm_args_on(xs->text());
      xs->tail("args");
    }
    if (Arguments::java_command() != NULL) {
      xs->head("command"); xs->text()->print_cr("%s", Arguments::java_command());
      xs->tail("command");
    }
    if (Arguments::sun_java_launcher() != NULL) {
      xs->head("launcher"); xs->text()->print_cr("%s", Arguments::sun_java_launcher());
      xs->tail("launcher");
    }
    if (Arguments::system_properties() !=  NULL) {
      xs->head("properties");
      // Print it as a java-style property list.
      // System properties don't generally contain newlines, so don't bother with unparsing.
      for (SystemProperty* p = Arguments::system_properties(); p != NULL; p = p->next()) {
        xs->text()->print_cr("%s=%s", p->key(), p->value());
      }
      xs->tail("properties");
    }
    xs->tail("vm_arguments");
    // tty output per se is grouped under the <tty>...</tty> element.
    xs->head("tty");
    // All further non-markup text gets copied to the tty:
    xs->_text = this;  // requires friend declaration!
  } else {
    delete(file);
    // and leave xtty as NULL
    LogVMOutput = false;
    DisplayVMOutput = true;
  }
}

// finish_log() is called during normal VM shutdown. finish_log_on_error() is
// called by ostream_abort() after a fatal error.
//
void defaultStream::finish_log() {
  xmlStream* xs = _outer_xmlStream;
  xs->done("tty");

  xs->done("hotspot_log");
  xs->flush();

  fileStream* file = _log_file;
  _log_file = NULL;

  delete _outer_xmlStream;
  _outer_xmlStream = NULL;
  
  file->flush();
  delete file;
}

void defaultStream::finish_log_on_error(char *buf, int buflen) {
  xmlStream* xs = _outer_xmlStream;

  if (xs && xs->out()) {

    xs->done_raw("tty");

    xs->done_raw("hotspot_log");
    xs->flush();

    fileStream* file = _log_file;
    _log_file = NULL;
    _outer_xmlStream = NULL;
  
    if (file) {
      file->flush();

      // Can't delete or close the file because delete and fclose aren't 
      // async-safe. We are about to die, so leave it to the kernel.
      // delete file;
    }
  }
}

intx defaultStream::hold(intx writer_id) {
  bool has_log = has_log_file();  // check before locking
  if (// impossible, but who knows?
      writer_id == NO_WRITER || 

      // developer hook
      !SerializeVMOutput || 

      // VM already unhealthy
      is_error_reported() || 
      
      // safepoint == global lock (for VM only)
      (SafepointSynchronize::is_synchronizing() && 
       Thread::current()->is_VM_thread()) 
      ) {
    // do not attempt to lock unless we know the thread and the VM is healthy
    return NO_WRITER;
  }
  if (_writer == writer_id) {
    // already held, no need to re-grab the lock
    return NO_WRITER;
  }
  // Stopped in the debugger somewhere already holding the lock?
  if( tty_lock.owned_by_self() ) return NO_WRITER;
  // Will fail internal deadlock asserts?
#ifdef ASSERT
  if( !Thread::current()->chk_lock(&tty_lock,false) ) return NO_WRITER;
#endif
  tty_lock.lock_can_block_gc(Thread::current());
  GET_RPC;
  tty_lock._rpc = RPC;
  // got the lock
  if (writer_id != _last_writer) {
    if (has_log) {
      _log_file->bol();
      // output a hint where this output is coming from:
      _log_file->print_cr("<writer thread='"INTX_FORMAT"'/>", writer_id);
    }
    _last_writer = writer_id;
  }
  _writer = writer_id;
  return writer_id;
}

void defaultStream::release(intx holder) {
  if (holder == NO_WRITER) {
    // nothing to release:  either a recursive lock, or we scribbled (too bad)
    return;
  }
  if (_writer != holder) {
    return;  // already unlocked, perhaps via break_tty_lock_for_safepoint
  }
  _writer = NO_WRITER;
tty_lock.unlock();
}


// Yuck:  jio_print does not accept char*/len.
static void call_jio_print(const char* s, size_t len) {
  char buffer[O_BUFLEN+100];
  if (len > sizeof(buffer)-1) {
    warning("increase O_BUFLEN in ostream.cpp -- output truncated");
    len = sizeof(buffer)-1;
  }
  strncpy(buffer, s, len);
  buffer[len] = '\0';
  jio_print(buffer);
}


void defaultStream::write(const char* s, size_t len) {
  intx thread_id = os::current_thread_id();
  intx holder = hold(thread_id);

  if (DisplayVMOutput &&
      (_outer_xmlStream == NULL || !_outer_xmlStream->inside_attrs())) {
    // print to output stream. It can be redirected by a vfprintf hook
    if (s[len] == '\0') {
      jio_print(s);
    } else {
      call_jio_print(s, len);
    }
  }

  // print to log file
  if (has_log_file()) {
    int nl0 = _newlines;
    xmlTextStream::write(s, len);
    // flush the log file too, if there were any newlines
    if (nl0 != _newlines){
      flush();
    }
  }
  update_position(s, len);

  release(holder);
}

intx ttyLocker::hold_tty() {
  if (defaultStream::instance == NULL)  return defaultStream::NO_WRITER;
  intx thread_id = os::current_thread_id();
  return defaultStream::instance->hold(thread_id);
}

void ttyLocker::release_tty(intx holder) {
  if (holder == defaultStream::NO_WRITER)  return;
  defaultStream::instance->release(holder);
}

void ttyLocker::break_tty_lock_for_safepoint(intx holder) {
  if (defaultStream::instance != NULL &&
      defaultStream::instance->writer() == holder) {
    if (xtty != NULL) {
      xtty->print_cr("<!-- safepoint while printing -->");
    }
    defaultStream::instance->release(holder);
  }
  // (else there was no lock to break)
}

void ostream_init() {
  if (defaultStream::instance == NULL) {
    defaultStream::instance = new(ResourceObj::C_HEAP) defaultStream();
    tty = defaultStream::instance;

    // We want to ensure that time stamps in GC logs consider time 0
    // the time when the JVM is initialized, not the first time we ask
    // for a time stamp. So, here, we explicitly update the time stamp
    // of tty.
tty->time_stamp().update();
  }

gclog=new(ResourceObj::C_HEAP)gclogStream();
}

void ostream_init_log() {
  // For -Xloggc:<file> option - called in runtime/thread.cpp
  // Note : this must be called AFTER ostream_init()

  gclog_or_tty = tty; // default to tty
  if (Arguments::gc_log_filename() != NULL) {
gclog->open(Arguments::gc_log_filename());
    if (gclog->is_open()) {
      // now we update the time stamp of the GC log to be synced up
      // with tty.
      gclog->time_stamp().update_to(tty->time_stamp().ticks());
      gclog_or_tty = gclog;
      
      // Make the GC log a buffered output
const char*format="GC log: %s";
      size_t buf_len = strlen(format) + strlen(Arguments::gc_log_filename()) + 1;
char*buf=(char*)malloc(buf_len);
      snprintf(buf, buf_len, format, Arguments::gc_log_filename());
#ifdef AZ_PROFILER
      gc_buf = new azprof::IORingBuffer(buf, 16*K);
      azprof::IORingBuffer::add(gc_buf);
#endif // AZ_PROFILER
free(buf);
    }
  }

  cpilog_or_tty = tty; // default to tty.
  if (strcmp(TraceCPIFileName, "")) {
fileStream*cpilog=new(ResourceObj::C_HEAP)fileStream(TraceCPIFileName);
if(cpilog->is_open()){
cpilog->time_stamp().update_to(tty->time_stamp().ticks());
          cpilog_or_tty = cpilog;
      }
  }
  
  // If we haven't lazily initialized the logfile yet, do it now,
  // to avoid the possibility of lazy initialization during a VM
  // crash, which can affect the stability of the fatal error handler.
  defaultStream::instance->has_log_file();
  
  if (ProfilerLogGC) {
    Unimplemented();
    //gclog_or_tty = new(ResourceObj::C_HEAP) externalProfilerGCStream();
  }
}

// ostream_exit() is called during normal VM exit to finish log files, flush 
// output and free resource.
void ostream_exit() {
static int ostream_exit_called=0;
if(Atomic::cmpxchg(1,&ostream_exit_called,0)!=0)
return;
if((gclog_or_tty!=tty)&&(gclog_or_tty!=gclog)){
    outputStream *tmp = gclog_or_tty;
    gclog_or_tty = NULL;
    delete tmp;
  }
  {
      // we temporaly disable PrintMallocFree here
      // as otherwise it'll lead to using of almost deleted
      // tty or defaultStream::instance in logging facility 
      // of HeapFree(), see 6391258
      DEBUG_ONLY(FlagSetting fs(PrintMallocFree, false);)
      if (tty != defaultStream::instance) {
outputStream*tmp=tty;
        tty = NULL;
        delete tmp;
      }
      if (defaultStream::instance != NULL) {
        outputStream *tmp = defaultStream::instance;
        defaultStream::instance = NULL;
        delete tmp;
      }
  }
  outputStream *tmp = gclog;
gclog=NULL;
  delete tmp;
  xtty = NULL;
}

// ostream_abort() is called by os::abort() when VM is about to die.
void ostream_abort() {
  // Here we can't delete gclog_or_tty and tty, just flush their output
  if (gclog_or_tty) gclog_or_tty->flush();
  if (tty) tty->flush();

  if (defaultStream::instance != NULL) {
    static char buf[4096];
    defaultStream::instance->finish_log_on_error(buf, sizeof(buf));
  }
}

staticBufferStream::staticBufferStream(char* buffer, size_t buflen,
				       outputStream *outer_stream) {
  _buffer = buffer;
  _buflen = buflen;
  _outer_stream = outer_stream;
}

void staticBufferStream::write(const char* c, size_t len) {
  _outer_stream->print_raw(c, (int)len);
}

void staticBufferStream::flush() {
  _outer_stream->flush();
}

void staticBufferStream::print(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, ap, false, len);
  write(str, len);
  va_end(ap);
}

void staticBufferStream::print_cr(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, ap, true, len);
  write(str, len);
  va_end(ap);
}

void staticBufferStream::vprint(const char *format, va_list argptr) {
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, argptr, false, len);
  write(str, len);
}

void staticBufferStream::vprint_cr(const char* format, va_list argptr) {
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, argptr, true, len);
  write(str, len);
}

bufferedStream::bufferedStream(size_t initial_size) : outputStream() {
  buffer_length = initial_size;
  buffer        = NEW_C_HEAP_ARRAY(char, buffer_length);
  buffer_pos    = 0;
  buffer_fixed  = false;
}
                                                                                                
bufferedStream::bufferedStream(char* fixed_buffer, size_t fixed_buffer_size) : outputStream() {
  buffer_length = fixed_buffer_size;
  buffer        = fixed_buffer;
  buffer_pos    = 0;
  buffer_fixed  = true;
}

void bufferedStream::write(const char* s, size_t len) {
  size_t end = buffer_pos + len;
  if (end >= buffer_length) {
    if (buffer_fixed) {
      // if buffer cannot resize, silently truncate
      len = buffer_length - buffer_pos - 1;
    } else {
      // For small overruns, double the buffer.  For larger ones, 
      // increase to the requested size.
      if (end < buffer_length * 2) {
        end = buffer_length * 2;
      }
      buffer = REALLOC_C_HEAP_ARRAY(char, buffer, end);
      buffer_length = end;
    }
  }
  memcpy(buffer + buffer_pos, s, len);
  buffer_pos += len;
}
                                                                                                
char* bufferedStream::as_string() {
  char* copy = NEW_RESOURCE_ARRAY(char, buffer_pos+1);
  strncpy(copy, buffer, buffer_pos);
  copy[buffer_pos] = 0;  // terminating null
  return copy;
}
                                                                                                
bufferedStream::~bufferedStream() {
  if (!buffer_fixed) {
    FREE_C_HEAP_ARRAY(char, buffer);
  }
}

