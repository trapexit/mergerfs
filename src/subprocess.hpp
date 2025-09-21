/*!

Documentation for C++ subprocessing library.

@copyright The code is licensed under the [MIT
  License](http://opensource.org/licenses/MIT):
  <br>
  Copyright &copy; 2016-2018 Arun Muralidharan.
  <br>
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  <br>
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  <br>
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

@author [Arun Muralidharan]
@see https://github.com/arun11299/cpp-subprocess to download the source code

@version 1.0.0
*/

#ifndef SUBPROCESS_HPP
#define SUBPROCESS_HPP

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <future>
#include <initializer_list>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if (defined _MSC_VER) || (defined __MINGW32__)
  #define __USING_WINDOWS__
#endif

#ifdef __USING_WINDOWS__
  #include <codecvt>
#endif

extern "C" {
#ifdef __USING_WINDOWS__
  #include <windows.h>
  #include <io.h>
  #include <cwchar>
#else
  #include <sys/wait.h>
  #include <unistd.h>
#endif
  #include <csignal>
  #include <fcntl.h>
  #include <sys/types.h>
}

// The Microsoft C++ compiler issues deprecation warnings
// for the standard POSIX function names.
// Its preferred implementations have a leading underscore.
// See: https://learn.microsoft.com/en-us/cpp/c-runtime-library/compatibility.
#if (defined _MSC_VER)
  #define subprocess_close _close
  #define subprocess_fileno _fileno
  #define subprocess_open _open
  #define subprocess_write _write
#else
  #define subprocess_close close
  #define subprocess_fileno fileno
  #define subprocess_open open
  #define subprocess_write write
#endif

/*!
 * Getting started with reading this source code.
 * The source is mainly divided into four parts:
 * 1. Exception Classes:
 *    These are very basic exception classes derived from
 *    runtime_error exception.
 *    There are two types of exception thrown from subprocess
 *    library: OSError and CalledProcessError
 *
 * 2. Popen Class
 *    This is the main class the users will deal with. It
 *    provides with all the API's to deal with processes.
 *
 * 3. Util namespace
 *    It includes some helper functions to split/join a string,
 *    reading from file descriptors, waiting on a process, fcntl
 *    options on file descriptors etc.
 *
 * 4. Detail namespace
 *    This includes some metaprogramming and helper classes.
 */


namespace subprocess {

// Max buffer size allocated on stack for read error
// from pipe
static const size_t SP_MAX_ERR_BUF_SIZ = 1024;

// Default buffer capacity for OutBuffer and ErrBuffer.
// If the data exceeds this capacity, the buffer size is grown
// by 1.5 times its previous capacity
static const size_t DEFAULT_BUF_CAP_BYTES = 8192;


/*-----------------------------------------------
 *    EXCEPTION CLASSES
 *-----------------------------------------------
 */

/*!
 * class: CalledProcessError
 * Thrown when there was error executing the command.
 * Check Popen class API's to know when this exception
 * can be thrown.
 *
 */
class CalledProcessError: public std::runtime_error
{
public:
  int retcode;
  CalledProcessError(const std::string& error_msg, int retcode):
    std::runtime_error(error_msg), retcode(retcode)
  {}
};


/*!
 * class: OSError
 * Thrown when some system call fails to execute or give result.
 * The exception message contains the name of the failed system call
 * with the stringisized errno code.
 * Check Popen class API's to know when this exception would be
 * thrown.
 * Its usual that the API exception specification would have
 * this exception together with CalledProcessError.
 */
class OSError: public std::runtime_error
{
public:
  OSError(const std::string& err_msg, int err_code):
    std::runtime_error( err_msg + ": " + std::strerror(err_code) )
  {}
};

//--------------------------------------------------------------------

//Environment Variable types
#ifndef __USING_WINDOWS__
	using platform_str_t = std::string;
	using platform_char_t = char;
#else
	using platform_str_t = std::wstring;
	using platform_char_t = wchar_t;
#endif
using env_str_t = platform_str_t;
using env_char_t = platform_char_t;
using env_map_t = std::map<platform_str_t, platform_str_t>;
using env_vector_t = std::vector<platform_char_t>;

//--------------------------------------------------------------------
namespace util
{
#ifdef __USING_WINDOWS__
  inline void quote_argument(const std::wstring &argument, std::wstring &command_line,
                      bool force)
  {
    //
    // Unless we're told otherwise, don't quote unless we actually
    // need to do so --- hopefully avoid problems if programs won't
    // parse quotes properly
    //

    if (force == false && argument.empty() == false &&
        argument.find_first_of(L" \t\n\v") == argument.npos) {
      command_line.append(argument);
    }
    else {
      command_line.push_back(L'"');

      for (auto it = argument.begin();; ++it) {
        unsigned number_backslashes = 0;

        while (it != argument.end() && *it == L'\\') {
          ++it;
          ++number_backslashes;
        }

        if (it == argument.end()) {

          //
          // Escape all backslashes, but let the terminating
          // double quotation mark we add below be interpreted
          // as a metacharacter.
          //

          command_line.append(number_backslashes * 2, L'\\');
          break;
        }
        else if (*it == L'"') {

          //
          // Escape all backslashes and the following
          // double quotation mark.
          //

          command_line.append(number_backslashes * 2 + 1, L'\\');
          command_line.push_back(*it);
        }
        else {

          //
          // Backslashes aren't special here.
          //

          command_line.append(number_backslashes, L'\\');
          command_line.push_back(*it);
        }
      }

      command_line.push_back(L'"');
    }
  }

  inline std::string get_last_error(DWORD errorMessageID)
  {
    if (errorMessageID == 0)
      return std::string();

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    LocalFree(messageBuffer);

    return message;
  }

  inline FILE *file_from_handle(HANDLE h, const char *mode)
  {
    int md;
    if (!mode) {
      throw OSError("invalid_mode", 0);
    }

    if (mode[0] == 'w') {
      md = _O_WRONLY;
    }
    else if (mode[0] == 'r') {
      md = _O_RDONLY;
    }
    else {
      throw OSError("file_from_handle", 0);
    }

    int os_fhandle = _open_osfhandle((intptr_t)h, md);
    if (os_fhandle == -1) {
      CloseHandle(h);
      throw OSError("_open_osfhandle", 0);
    }

    FILE *fp = _fdopen(os_fhandle, mode);
    if (fp == 0) {
      subprocess_close(os_fhandle);
      throw OSError("_fdopen", 0);
    }

    return fp;
  }

  inline void configure_pipe(HANDLE* read_handle, HANDLE* write_handle, HANDLE* child_handle)
  {
    SECURITY_ATTRIBUTES saAttr;

    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDIN.
    if (!CreatePipe(read_handle, write_handle, &saAttr,0))
      throw OSError("CreatePipe", 0);

    // Ensure the write handle to the pipe for STDIN is not inherited.
    if (!SetHandleInformation(*child_handle, HANDLE_FLAG_INHERIT, 0))
      throw OSError("SetHandleInformation", 0);
  }

  // env_map_t MapFromWindowsEnvironment()
  // * Imports current Environment in a C-string table
  // * Parses the strings by splitting on the first "=" per line
  // * Creates a map of the variables
  // * Returns the map
  inline env_map_t MapFromWindowsEnvironment(){
      wchar_t *variable_strings_ptr;
      wchar_t *environment_strings_ptr;
      std::wstring delimiter(L"=");
      int del_len = delimiter.length();
      env_map_t mapped_environment;

      // Get a pointer to the environment block.
      environment_strings_ptr = GetEnvironmentStringsW();
      // If the returned pointer is NULL, exit.
      if (environment_strings_ptr == NULL)
      {
          throw OSError("GetEnvironmentStringsW", 0);
      }

      // Variable strings are separated by NULL byte, and the block is
      // terminated by a NULL byte.

      variable_strings_ptr = (wchar_t *) environment_strings_ptr;

      //Since the environment map ends with a null, we can loop until we find it.
      while (*variable_strings_ptr)
      {
          // Create a string from Variable String
          platform_str_t current_line(variable_strings_ptr);
          // Find the first "equals" sign.
          auto pos = current_line.find(delimiter);
          // Assuming it's not missing ...
          if(pos!=std::wstring::npos){
              // ... parse the key and value.
              platform_str_t key = current_line.substr(0, pos);
              platform_str_t value = current_line.substr(pos + del_len);
              // Map the entry.
              mapped_environment[key] = value;
          }
          // Jump to next line in the environment map.
          variable_strings_ptr += std::wcslen(variable_strings_ptr) + 1;
      }
      // We're done with the old environment map buffer.
      FreeEnvironmentStringsW(environment_strings_ptr);

      // Return the map.
      return mapped_environment;
  }

  // env_vector_t WindowsEnvironmentVectorFromMap(const env_map_t &source_map)
  // * Creates a vector buffer for the new environment string table
  // * Copies in the mapped variables
  // * Returns the vector
  inline env_vector_t WindowsEnvironmentVectorFromMap(const env_map_t &source_map)
  {
	// Make a new environment map buffer.
	env_vector_t environment_map_buffer;
	// Give it some space.
	environment_map_buffer.reserve(4096);

	// And fill'er up.
	for(auto kv: source_map){
	  // Create the line
	  platform_str_t current_line(kv.first); current_line += L"="; current_line += kv.second;
	  // Add the line to the buffer.
	  std::copy(current_line.begin(), current_line.end(), std::back_inserter(environment_map_buffer));
	  // Append a null
	  environment_map_buffer.push_back(0);
	}
	// Append one last null because of how Windows does it's environment maps.
	environment_map_buffer.push_back(0);

	return environment_map_buffer;
  }

  // env_vector_t CreateUpdatedWindowsEnvironmentVector(const env_map_t &changes_map)
  // * Merges host environment with new mapped variables
  // * Creates and returns string vector based on map
  inline env_vector_t CreateUpdatedWindowsEnvironmentVector(const env_map_t &changes_map){
	// Import the environment map
	env_map_t environment_map = MapFromWindowsEnvironment();
    // Merge in the changes with overwrite
	for(auto& it: changes_map)
	{
		environment_map[it.first] = it.second;
	}
    // Create a Windows-usable Environment Map Buffer
    env_vector_t environment_map_strings_vector = WindowsEnvironmentVectorFromMap(environment_map);

	return environment_map_strings_vector;
  }

#endif

  /*!
   * Function: split
   * Parameters:
   * [in] str : Input string which needs to be split based upon the
   *            delimiters provided.
   * [in] deleims : Delimiter characters based upon which the string needs
   *                to be split. Default constructed to ' '(space) and '\t'(tab)
   * [out] vector<string> : Vector of strings split at deleimiter.
   */
  static inline std::vector<std::string>
  split(const std::string& str, const std::string& delims=" \t")
  {
    std::vector<std::string> res;
    size_t init = 0;

    while (true) {
      auto pos = str.find_first_of(delims, init);
      if (pos == std::string::npos) {
        res.emplace_back(str.substr(init, str.length()));
        break;
      }
      res.emplace_back(str.substr(init, pos - init));
      pos++;
      init = pos;
    }

    return res;
  }


  /*!
   * Function: join
   * Parameters:
   * [in] vec : Vector of strings which needs to be joined to form
   *            a single string with words separated by a separator char.
   *  [in] sep : String used to separate 2 words in the joined string.
   *             Default constructed to ' ' (space).
   *  [out] string: Joined string.
   */
  static inline
  std::string join(const std::vector<std::string>& vec,
                   const std::string& sep = " ")
  {
    std::string res;
    for (auto& elem : vec) res.append(elem + sep);
    res.erase(--res.end());
    return res;
  }


#ifndef __USING_WINDOWS__
  /*!
   * Function: set_clo_on_exec
   * Sets/Resets the FD_CLOEXEC flag on the provided file descriptor
   * based upon the `set` parameter.
   * Parameters:
   * [in] fd : The descriptor on which FD_CLOEXEC needs to be set/reset.
   * [in] set : If 'true', set FD_CLOEXEC.
   *            If 'false' unset FD_CLOEXEC.
   */
  static inline
  void set_clo_on_exec(int fd, bool set = true)
  {
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags == -1) {
        throw OSError("fcntl F_GETFD failed", errno);
    }
    if (set) flags |= FD_CLOEXEC;
    else flags &= ~FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
        throw OSError("fcntl F_SETFD failed", errno);
    }
  }


  /*!
   * Function: pipe_cloexec
   * Creates a pipe and sets FD_CLOEXEC flag on both
   * read and write descriptors of the pipe.
   * Parameters:
   * [out] : A pair of file descriptors.
   *         First element of pair is the read descriptor of pipe.
   *         Second element is the write descriptor of pipe.
   */
  static inline
  std::pair<int, int> pipe_cloexec() noexcept(false)
  {
    int pipe_fds[2];
    int res = pipe(pipe_fds);
    if (res) {
      throw OSError("pipe failure", errno);
    }

    set_clo_on_exec(pipe_fds[0]);
    set_clo_on_exec(pipe_fds[1]);

    return std::make_pair(pipe_fds[0], pipe_fds[1]);
  }
#endif


  /*!
   * Function: write_n
   * Writes `length` bytes to the file descriptor `fd`
   * from the buffer `buf`.
   * Parameters:
   * [in] fd : The file descriptotr to write to.
   * [in] buf: Buffer from which data needs to be written to fd.
   * [in] length: The number of bytes that needs to be written from
   *              `buf` to `fd`.
   * [out] int : Number of bytes written or -1 in case of failure.
   */
  static inline
  int write_n(int fd, const char* buf, size_t length)
  {
    size_t nwritten = 0;
    while (nwritten < length) {
      int written = subprocess_write(fd, buf + nwritten, length - nwritten);
      if (written == -1) return -1;
      nwritten += written;
    }
    return nwritten;
  }


  /*!
   * Function: read_atmost_n
   * Reads at the most `read_upto` bytes from the
   * file object `fp` before returning.
   * Parameters:
   * [in] fp : The file object from which it needs to read.
   * [in] buf : The buffer into which it needs to write the data.
   * [in] read_upto: Max number of bytes which must be read from `fd`.
   * [out] int : Number of bytes written to `buf` or read from `fd`
   *             OR -1 in case of error.
   *  NOTE: In case of EINTR while reading from socket, this API
   *  will retry to read from `fd`, but only till the EINTR counter
   *  reaches 50 after which it will return with whatever data it read.
   */
  static inline
  int read_atmost_n(FILE* fp, char* buf, size_t read_upto)
  {
#ifdef __USING_WINDOWS__
    return (int)fread(buf, 1, read_upto, fp);
#else
    int fd = subprocess_fileno(fp);
    int rbytes = 0;
    int eintr_cnter = 0;

    while (1) {
      int read_bytes = read(fd, buf + rbytes, read_upto - rbytes);
      if (read_bytes == -1) {
        if (errno == EINTR) {
          if (eintr_cnter >= 50) return -1;
          eintr_cnter++;
          continue;
        }
        return -1;
      }
      if (read_bytes == 0) return rbytes;

      rbytes += read_bytes;
    }
    return rbytes;
#endif
  }


  /*!
   * Function: read_all
   * Reads all the available data from `fp` into
   * `buf`. Internally calls read_atmost_n.
   * Parameters:
   * [in] fp : The file object from which to read from.
   * [in] buf : The buffer of type `class Buffer` into which
   *            the read data is written to.
   * [out] int: Number of bytes read OR -1 in case of failure.
   *
   * NOTE: `class Buffer` is a exposed public class. See below.
   */

  static inline int read_all(FILE* fp, std::vector<char>& buf)
  {
    auto buffer = buf.data();
    int total_bytes_read = 0;
    int fill_sz = buf.size();

    while (1) {
      const int rd_bytes = read_atmost_n(fp, buffer, fill_sz);

      if (rd_bytes == -1) { // Read finished
        if (total_bytes_read == 0) return -1;
        break;

      } else if (rd_bytes == fill_sz) { // Buffer full
        const auto orig_sz = buf.size();
        const auto new_sz = orig_sz * 2;
        buf.resize(new_sz);
        fill_sz = new_sz - orig_sz;

        //update the buffer pointer
        buffer = buf.data();
        total_bytes_read += rd_bytes;
        buffer += total_bytes_read;

      } else { // Partial data ? Continue reading
        total_bytes_read += rd_bytes;
        fill_sz -= rd_bytes;
        break;
      }
    }
    buf.erase(buf.begin()+total_bytes_read, buf.end()); // remove extra nulls
    return total_bytes_read;
  }

#ifndef __USING_WINDOWS__
  /*!
   * Function: wait_for_child_exit
   * Waits for the process with pid `pid` to exit
   * and returns its status.
   * Parameters:
   * [in] pid : The pid of the process.
   * [out] pair<int, int>:
   *    pair.first : Return code of the waitpid call.
   *    pair.second : Exit status of the process.
   *
   *  NOTE: This is a blocking call as in, it will loop
   *  till the child is exited.
   */
  static inline
  std::pair<int, int> wait_for_child_exit(int pid)
  {
    int status = 0;
    int ret = -1;
    while (1) {
      ret = waitpid(pid, &status, 0);
      if (ret == -1) break;
      if (ret == 0) continue;
      return std::make_pair(ret, status);
    }

    return std::make_pair(ret, status);
  }
#endif

} // end namespace util



/* -------------------------------
 *     Popen Arguments
 * -------------------------------
 */

/*!
 * The buffer size of the stdin/stdout/stderr
 * streams of the child process.
 * Default value is 0.
 */
struct bufsize {
  explicit bufsize(int sz): bufsiz(sz) {}
  int  bufsiz = 0;
};

/*!
 * Option to defer spawning of the child process
 * till `Popen::start_process` API is called.
 * Default value is false.
 */
struct defer_spawn {
  explicit defer_spawn(bool d): defer(d) {}
  bool defer  = false;
};

/*!
 * Option to close all file descriptors
 * when the child process is spawned.
 * The close fd list does not include
 * input/output/error if they are explicitly
 * set as part of the Popen arguments.
 *
 * Default value is false.
 */
struct close_fds {
  explicit close_fds(bool c): close_all(c) {}
  bool close_all = false;
};

/*!
 * Option to make the child process as the
 * session leader and thus the process
 * group leader.
 * Default value is false.
 */
struct session_leader {
  explicit session_leader(bool sl): leader_(sl) {}
  bool leader_ = false;
};

struct shell {
  explicit shell(bool s): shell_(s) {}
  bool shell_ = false;
};

/*!
 * Base class for all arguments involving string value.
 */
struct string_arg
{
  string_arg(const char* arg): arg_value(arg) {}
  string_arg(std::string&& arg): arg_value(std::move(arg)) {}
  string_arg(const std::string& arg): arg_value(arg) {}
  std::string arg_value;
};

/*!
 * Option to specify the executable name separately
 * from the args sequence.
 * In this case the cmd args must only contain the
 * options required for this executable.
 *
 * Eg: executable{"ls"}
 */
struct executable: string_arg
{
  template <typename T>
  executable(T&& arg): string_arg(std::forward<T>(arg)) {}
};

/*!
 * Option to set the current working directory
 * of the spawned process.
 *
 * Eg: cwd{"/some/path/x"}
 */
struct cwd
{
  explicit cwd(const platform_str_t &cwd): cwd_(cwd) {}
  explicit cwd(platform_str_t &&cwd): cwd_(std::move(cwd)) {}
  platform_str_t cwd_;
};

/*!
 * Option to specify environment variables required by
 * the spawned process.
 *
 * Eg: environment{{ {"K1", "V1"}, {"K2", "V2"},... }}
 */
struct environment
{
  environment(env_map_t&& env):
    env_(std::move(env)) {}
  explicit environment(const env_map_t& env):
    env_(env) {}
  env_map_t env_;
};


/*!
 * Used for redirecting input/output/error
 */
enum IOTYPE {
  STDOUT = 1,
  STDERR,
  PIPE,
};

//TODO: A common base/interface for below stream structures ??

/*!
 * Option to specify the input channel for the child
 * process. It can be:
 * 1. An already open file descriptor.
 * 2. A file name.
 * 3. IOTYPE. Usual a PIPE
 *
 * Eg: input{PIPE}
 * OR in case of redirection, output of another Popen
 * input{popen.output()}
 */
struct input
{
  // For an already existing file descriptor.
  explicit input(int fd): rd_ch_(fd) {}

  // FILE pointer.
  explicit input (FILE* fp):input(subprocess_fileno(fp)) { assert(fp); }

  explicit input(const char* filename) {
    int fd = subprocess_open(filename, O_RDONLY);
    if (fd == -1) throw OSError("File not found: ", errno);
    rd_ch_ = fd;
  }
  explicit input(IOTYPE typ) {
    assert (typ == PIPE && "STDOUT/STDERR not allowed");
#ifndef __USING_WINDOWS__
    std::tie(rd_ch_, wr_ch_) = util::pipe_cloexec();
#endif
  }

  int rd_ch_ = -1;
  int wr_ch_ = -1;
};


/*!
 * Option to specify the output channel for the child
 * process. It can be:
 * 1. An already open file descriptor.
 * 2. A file name.
 * 3. IOTYPE. Usually a PIPE.
 *
 * Eg: output{PIPE}
 * OR output{"output.txt"}
 */
struct output
{
  explicit output(int fd): wr_ch_(fd) {}

  explicit output (FILE* fp):output(subprocess_fileno(fp)) { assert(fp); }

  explicit output(const char* filename) {
    int fd = subprocess_open(filename, O_APPEND | O_CREAT | O_RDWR, 0640);
    if (fd == -1) throw OSError("File not found: ", errno);
    wr_ch_ = fd;
  }
  explicit output(IOTYPE typ) {
    assert (typ == PIPE && "STDOUT/STDERR not allowed");
#ifndef __USING_WINDOWS__
    std::tie(rd_ch_, wr_ch_) = util::pipe_cloexec();
#endif
  }

  int rd_ch_ = -1;
  int wr_ch_ = -1;
};


/*!
 * Option to specify the error channel for the child
 * process. It can be:
 * 1. An already open file descriptor.
 * 2. A file name.
 * 3. IOTYPE. Usually a PIPE or STDOUT
 *
 */
struct error
{
  explicit error(int fd): wr_ch_(fd) {}

  explicit error(FILE* fp):error(subprocess_fileno(fp)) { assert(fp); }

  explicit error(const char* filename) {
    int fd = subprocess_open(filename, O_APPEND | O_CREAT | O_RDWR, 0640);
    if (fd == -1) throw OSError("File not found: ", errno);
    wr_ch_ = fd;
  }
  explicit error(IOTYPE typ) {
    assert ((typ == PIPE || typ == STDOUT) && "STDERR not allowed");
    if (typ == PIPE) {
#ifndef __USING_WINDOWS__
      std::tie(rd_ch_, wr_ch_) = util::pipe_cloexec();
#endif
    } else {
      // Need to defer it till we have checked all arguments
      deferred_ = true;
    }
  }

  bool deferred_ = false;
  int rd_ch_ = -1;
  int wr_ch_ = -1;
};

// Impoverished, meager, needy, truly needy
// version of type erasure to store function pointers
// needed to provide the functionality of preexec_func
// ATTN: Can be used only to execute functions with no
// arguments and returning void.
// Could have used more efficient methods, ofcourse, but
// that won't yield me the consistent syntax which I am
// aiming for. If you know, then please do let me know.

class preexec_func
{
public:
  preexec_func() {}

  template <typename Func>
  explicit preexec_func(Func f): holder_(new FuncHolder<Func>(std::move(f)))
  {}

  void operator()() {
    (*holder_)();
  }

private:
  struct HolderBase {
    virtual void operator()() const = 0;
    virtual ~HolderBase(){};
  };
  template <typename T>
  struct FuncHolder: HolderBase {
    FuncHolder(T func): func_(std::move(func)) {}
    void operator()() const override { func_(); }
    // The function pointer/reference
    T func_;
  };

  std::unique_ptr<HolderBase> holder_ = nullptr;
};

// ~~~~ End Popen Args ~~~~


/*!
 * class: Buffer
 * This class is a very thin wrapper around std::vector<char>
 * This is basically used to determine the length of the actual
 * data stored inside the dynamically resized vector.
 *
 * This is what is returned as the output to communicate and check_output
 * functions, so, users must know about this class.
 *
 * OutBuffer and ErrBuffer are just different typedefs to this class.
 */
class Buffer
{
public:
  Buffer() {}
  explicit Buffer(size_t cap) { buf.resize(cap); }
  void add_cap(size_t cap) { buf.resize(cap); }

#if 0
  Buffer(const Buffer& other):
    buf(other.buf),
    length(other.length)
  {
    std::cout << "COPY" << std::endl;
  }

  Buffer(Buffer&& other):
    buf(std::move(other.buf)),
    length(other.length)
  {
    std::cout << "MOVE" << std::endl;
  }
#endif

public:
  std::vector<char> buf;
  size_t length = 0;
};

// Buffer for storing output written to output fd
using OutBuffer = Buffer;
// Buffer for storing output written to error fd
using ErrBuffer = Buffer;


// Fwd Decl.
class Popen;

/*---------------------------------------------------
 *      DETAIL NAMESPACE
 *---------------------------------------------------
 */

namespace detail {

// Metaprogram for searching a type within
// a variadic parameter pack
// This is particularly required to do a compile time
// checking of the arguments provided to 'check_ouput' function
// wherein the user is not expected to provide an 'ouput' option.

template <typename... T> struct param_pack{};

template <typename F, typename T> struct has_type;

template <typename F>
struct has_type<F, param_pack<>> {
  static constexpr bool value = false;
};

template <typename F, typename... T>
struct has_type<F, param_pack<F, T...>> {
  static constexpr bool value = true;
};

template <typename F, typename H, typename... T>
struct has_type<F, param_pack<H,T...>> {
  static constexpr bool value =
    std::is_same<F, typename std::decay<H>::type>::value ? true : has_type<F, param_pack<T...>>::value;
};

//----

/*!
 * A helper class to Popen class for setting
 * options as provided in the Popen constructor
 * or in check_ouput arguments.
 * This design allows us to _not_ have any fixed position
 * to any arguments and specify them in a way similar to what
 * can be done in python.
 */
struct ArgumentDeducer
{
  ArgumentDeducer(Popen* p): popen_(p) {}

  void set_option(executable&& exe);
  void set_option(cwd&& cwdir);
  void set_option(bufsize&& bsiz);
  void set_option(environment&& env);
  void set_option(defer_spawn&& defer);
  void set_option(shell&& sh);
  void set_option(input&& inp);
  void set_option(output&& out);
  void set_option(error&& err);
  void set_option(close_fds&& cfds);
  void set_option(preexec_func&& prefunc);
  void set_option(session_leader&& sleader);

private:
  Popen* popen_ = nullptr;
};

/*!
 * A helper class to Popen.
 * This takes care of all the fork-exec logic
 * in the execute_child API.
 */
class Child
{
public:
  Child(Popen* p, int err_wr_pipe):
    parent_(p),
    err_wr_pipe_(err_wr_pipe)
  {}

  void execute_child();

private:
  // Lets call it parent even though
  // technically a bit incorrect
  Popen* parent_ = nullptr;
  int err_wr_pipe_ = -1;
};

// Fwd Decl.
class Streams;

/*!
 * A helper class to Streams.
 * This takes care of management of communicating
 * with the child process with the means of the correct
 * file descriptor.
 */
class Communication
{
public:
  Communication(Streams* stream): stream_(stream)
  {}
  Communication(const Communication&) = delete;
  Communication& operator=(const Communication&) = delete;
  Communication(Communication&&) = default;
  Communication& operator=(Communication&&) = default;
public:
  int send(const char* msg, size_t length);
  int send(const std::vector<char>& msg);

  std::pair<OutBuffer, ErrBuffer> communicate(const char* msg, size_t length);
  std::pair<OutBuffer, ErrBuffer> communicate(const std::vector<char>& msg)
  { return communicate(msg.data(), msg.size()); }

  void set_out_buf_cap(size_t cap) { out_buf_cap_ = cap; }
  void set_err_buf_cap(size_t cap) { err_buf_cap_ = cap; }

private:
  std::pair<OutBuffer, ErrBuffer> communicate_threaded(
      const char* msg, size_t length);

private:
  Streams* stream_;
  size_t out_buf_cap_ = DEFAULT_BUF_CAP_BYTES;
  size_t err_buf_cap_ = DEFAULT_BUF_CAP_BYTES;
};



/*!
 * This is a helper class to Popen.
 * It takes care of management of all the file descriptors
 * and file pointers.
 * It dispatches of the communication aspects to the
 * Communication class.
 * Read through the data members to understand about the
 * various file descriptors used.
 */
class Streams
{
public:
  Streams():comm_(this) {}
  Streams(const Streams&) = delete;
  Streams& operator=(const Streams&) = delete;
  Streams(Streams&&) = default;
  Streams& operator=(Streams&&) = default;

public:
  void setup_comm_channels();

  void cleanup_fds()
  {
    if (write_to_child_ != -1 && read_from_parent_ != -1) {
      subprocess_close(write_to_child_);
    }
    if (write_to_parent_ != -1 && read_from_child_ != -1) {
      subprocess_close(read_from_child_);
    }
    if (err_write_ != -1 && err_read_ != -1) {
      subprocess_close(err_read_);
    }
  }

  void close_parent_fds()
  {
    if (write_to_child_ != -1)  subprocess_close(write_to_child_);
    if (read_from_child_ != -1) subprocess_close(read_from_child_);
    if (err_read_ != -1)        subprocess_close(err_read_);
  }

  void close_child_fds()
  {
    if (write_to_parent_ != -1)  subprocess_close(write_to_parent_);
    if (read_from_parent_ != -1) subprocess_close(read_from_parent_);
    if (err_write_ != -1)        subprocess_close(err_write_);
  }

  FILE* input()  { return input_.get(); }
  FILE* output() { return output_.get(); }
  FILE* error()  { return error_.get(); }

  void input(FILE* fp)  { input_.reset(fp, fclose); }
  void output(FILE* fp) { output_.reset(fp, fclose); }
  void error(FILE* fp)  { error_.reset(fp, fclose); }

  void set_out_buf_cap(size_t cap) { comm_.set_out_buf_cap(cap); }
  void set_err_buf_cap(size_t cap) { comm_.set_err_buf_cap(cap); }

public: /* Communication forwarding API's */
  int send(const char* msg, size_t length)
  { return comm_.send(msg, length); }

  int send(const std::vector<char>& msg)
  { return comm_.send(msg); }

  std::pair<OutBuffer, ErrBuffer> communicate(const char* msg, size_t length)
  { return comm_.communicate(msg, length); }

  std::pair<OutBuffer, ErrBuffer> communicate(const std::vector<char>& msg)
  { return comm_.communicate(msg); }


public:// Yes they are public

  std::shared_ptr<FILE> input_  = nullptr;
  std::shared_ptr<FILE> output_ = nullptr;
  std::shared_ptr<FILE> error_  = nullptr;

#ifdef __USING_WINDOWS__
  HANDLE g_hChildStd_IN_Rd = nullptr;
  HANDLE g_hChildStd_IN_Wr = nullptr;
  HANDLE g_hChildStd_OUT_Rd = nullptr;
  HANDLE g_hChildStd_OUT_Wr = nullptr;
  HANDLE g_hChildStd_ERR_Rd = nullptr;
  HANDLE g_hChildStd_ERR_Wr = nullptr;
#endif

  // Buffer size for the IO streams
  int bufsiz_ = 0;

  // Pipes for communicating with child

  // Emulates stdin
  int write_to_child_   = -1; // Parent owned descriptor
  int read_from_parent_ = -1; // Child owned descriptor

  // Emulates stdout
  int write_to_parent_ = -1; // Child owned descriptor
  int read_from_child_ = -1; // Parent owned descriptor

  // Emulates stderr
  int err_write_ = -1; // Write error to parent (Child owned)
  int err_read_  = -1; // Read error from child (Parent owned)

private:
  Communication comm_;
};

} // end namespace detail



/*!
 * class: Popen
 * This is the single most important class in the whole library
 * and glues together all the helper classes to provide a common
 * interface to the client.
 *
 * API's provided by the class:
 * 1. Popen({"cmd"}, output{..}, error{..}, cwd{..}, ....)
 *    Command provided as a sequence.
 * 2. Popen("cmd arg1"m output{..}, error{..}, cwd{..}, ....)
 *    Command provided in a single string.
 * 3. wait()             - Wait for the child to exit.
 * 4. retcode()          - The return code of the exited child.
 * 5. pid()              - PID of the spawned child.
 * 6. poll()             - Check the status of the running child.
 * 7. kill(sig_num)      - Kill the child. SIGTERM used by default.
 * 8. send(...)          - Send input to the input channel of the child.
 * 9. communicate(...)   - Get the output/error from the child and close the channels
 *                         from the parent side.
 *10. input()            - Get the input channel/File pointer. Can be used for
 *                         customizing the way of sending input to child.
 *11. output()           - Get the output channel/File pointer. Usually used
                           in case of redirection. See piping examples.
 *12. error()            - Get the error channel/File pointer. Usually used
                           in case of redirection.
 *13. start_process()    - Start the child process. Only to be used when
 *                         `defer_spawn` option was provided in Popen constructor.
 */
class Popen
{
public:
  friend struct detail::ArgumentDeducer;
  friend class detail::Child;

  template <typename... Args>
  Popen(const std::string& cmd_args, Args&& ...args):
    args_(cmd_args)
  {
    vargs_ = util::split(cmd_args);
    init_args(std::forward<Args>(args)...);

    // Setup the communication channels of the Popen class
    stream_.setup_comm_channels();

    if (!defer_process_start_) execute_process();
  }

  template <typename... Args>
  Popen(std::initializer_list<const char*> cmd_args, Args&& ...args)
  {
    vargs_.insert(vargs_.end(), cmd_args.begin(), cmd_args.end());
    init_args(std::forward<Args>(args)...);

    // Setup the communication channels of the Popen class
    stream_.setup_comm_channels();

    if (!defer_process_start_) execute_process();
  }

  template <typename... Args>
  Popen(std::vector<std::string> vargs_, Args &&... args) : vargs_(vargs_)
  {
    init_args(std::forward<Args>(args)...);

    // Setup the communication channels of the Popen class
    stream_.setup_comm_channels();

    if (!defer_process_start_) execute_process();
  }

/*
  ~Popen()
  {
#ifdef __USING_WINDOWS__
    CloseHandle(this->process_handle_);
#endif
  }
*/

  void start_process() noexcept(false);

  int pid() const noexcept { return child_pid_; }

  int retcode() const noexcept { return retcode_; }

  int wait() noexcept(false);

  int poll() noexcept(false);

  // Does not fail, Caller is expected to recheck the
  // status with a call to poll()
  void kill(int sig_num = 9);

  void set_out_buf_cap(size_t cap) { stream_.set_out_buf_cap(cap); }

  void set_err_buf_cap(size_t cap) { stream_.set_err_buf_cap(cap); }

  int send(const char* msg, size_t length)
  { return stream_.send(msg, length); }

  int send(const std::string& msg)
  { return send(msg.c_str(), msg.size()); }

  int send(const std::vector<char>& msg)
  { return stream_.send(msg); }

  std::pair<OutBuffer, ErrBuffer> communicate(const char* msg, size_t length)
  {
    auto res = stream_.communicate(msg, length);
    retcode_ = wait();
    return res;
  }

  std::pair<OutBuffer, ErrBuffer> communicate(const std::string& msg)
  {
    return communicate(msg.c_str(), msg.size());
  }

  std::pair<OutBuffer, ErrBuffer> communicate(const std::vector<char>& msg)
  {
    auto res = stream_.communicate(msg);
    retcode_ = wait();
    return res;
  }

  std::pair<OutBuffer, ErrBuffer> communicate()
  {
    return communicate(nullptr, 0);
  }

  FILE* input()  { return stream_.input(); }
  FILE* output() { return stream_.output();}
  FILE* error()  { return stream_.error(); }

  /// Stream close APIs
  void close_input()  { stream_.input_.reset();  }
  void close_output() { stream_.output_.reset(); }
  void close_error()  { stream_.error_.reset();  }

private:
  template <typename F, typename... Args>
  void init_args(F&& farg, Args&&... args);
  void init_args();
  void populate_c_argv();
  void execute_process() noexcept(false);

private:
  detail::Streams stream_;

#ifdef __USING_WINDOWS__
  HANDLE process_handle_;
  std::future<void> cleanup_future_;
#endif

  bool defer_process_start_ = false;
  bool close_fds_ = false;
  bool has_preexec_fn_ = false;
  bool shell_ = false;
  bool session_leader_ = false;

  std::string exe_name_;
  platform_str_t cwd_;
  env_map_t env_;
  preexec_func preexec_fn_;

  // Command in string format
  std::string args_;
  // Command provided as sequence
  std::vector<std::string> vargs_;
  std::vector<char*> cargv_;

  bool child_created_ = false;
  // Pid of the child process
  int child_pid_ = -1;

  int retcode_ = -1;
};

inline void Popen::init_args() {
  populate_c_argv();
}

template <typename F, typename... Args>
inline void Popen::init_args(F&& farg, Args&&... args)
{
  detail::ArgumentDeducer argd(this);
  argd.set_option(std::forward<F>(farg));
  init_args(std::forward<Args>(args)...);
}

inline void Popen::populate_c_argv()
{
  cargv_.clear();
  cargv_.reserve(vargs_.size() + 1);
  for (auto& arg : vargs_) cargv_.push_back(&arg[0]);
  cargv_.push_back(nullptr);
}

inline void Popen::start_process() noexcept(false)
{
  // The process was started/tried to be started
  // in the constructor itself.
  // For explicitly calling this API to start the
  // process, 'defer_spawn' argument must be set to
  // true in the constructor.
  if (!defer_process_start_) {
    assert (0);
    return;
  }
  execute_process();
}

inline int Popen::wait() noexcept(false)
{
#ifdef __USING_WINDOWS__
  int ret = WaitForSingleObject(process_handle_, INFINITE);

  // WaitForSingleObject with INFINITE should only return when process has signaled
  if (ret != WAIT_OBJECT_0) {
    throw OSError("Unexpected return code from WaitForSingleObject", 0);
  }

  DWORD dretcode_;

  if (FALSE == GetExitCodeProcess(process_handle_, &dretcode_))
      throw OSError("Failed during call to GetExitCodeProcess", 0);

  CloseHandle(process_handle_);

  return (int)dretcode_;
#else
  int ret, status;
  std::tie(ret, status) = util::wait_for_child_exit(pid());
  if (ret == -1) {
    if (errno != ECHILD) throw OSError("waitpid failed", errno);
    return 0;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return WTERMSIG(status);
  else return 255;

  return 0;
#endif
}

inline int Popen::poll() noexcept(false)
{
#ifdef __USING_WINDOWS__
  int ret = WaitForSingleObject(process_handle_, 0);
  if (ret != WAIT_OBJECT_0) return -1;

  DWORD dretcode_;
  if (FALSE == GetExitCodeProcess(process_handle_, &dretcode_))
      throw OSError("GetExitCodeProcess", 0);

  retcode_ = (int)dretcode_;
  CloseHandle(process_handle_);

  return retcode_;
#else
  if (!child_created_) return -1; // TODO: ??

  int status;

  // Returns zero if child is still running
  int ret = waitpid(child_pid_, &status, WNOHANG);
  if (ret == 0) return -1;

  if (ret == child_pid_) {
    if (WIFSIGNALED(status)) {
      retcode_ = WTERMSIG(status);
    } else if (WIFEXITED(status)) {
      retcode_ = WEXITSTATUS(status);
    } else {
      retcode_ = 255;
    }
    return retcode_;
  }

  if (ret == -1) {
    // From subprocess.py
    // This happens if SIGCHLD is set to be ignored
    // or waiting for child process has otherwise been disabled
    // for our process. This child is dead, we cannot get the
    // status.
    if (errno == ECHILD) retcode_ = 0;
    else throw OSError("waitpid failed", errno);
  } else {
    retcode_ = ret;
  }

  return retcode_;
#endif
}

inline void Popen::kill(int sig_num)
{
#ifdef __USING_WINDOWS__
  if (!TerminateProcess(this->process_handle_, (UINT)sig_num)) {
    throw OSError("TerminateProcess", 0);
  }
#else
  if (session_leader_) killpg(child_pid_, sig_num);
  else ::kill(child_pid_, sig_num);
#endif
}


inline void Popen::execute_process() noexcept(false)
{
#ifdef __USING_WINDOWS__
  if (this->shell_) {
    throw OSError("shell not currently supported on windows", 0);
  }

  void* environment_string_table_ptr = nullptr;
  env_vector_t environment_string_vector;
  if(this->env_.size()){
	  environment_string_vector = util::CreateUpdatedWindowsEnvironmentVector(env_);
	  environment_string_table_ptr = (void*)environment_string_vector.data();
  }

  if (exe_name_.length()) {
    this->vargs_.insert(this->vargs_.begin(), this->exe_name_);
    this->populate_c_argv();
  }
  this->exe_name_ = vargs_[0];

  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  std::wstring argument;
  std::wstring command_line;
  bool first_arg = true;

  for (auto arg : this->vargs_) {
    if (!first_arg) {
      command_line += L" ";
    } else {
      first_arg = false;
    }
    argument = converter.from_bytes(arg);
    util::quote_argument(argument, command_line, false);
  }

  // CreateProcessW can modify szCmdLine so we allocate needed memory
  wchar_t *szCmdline = new wchar_t[command_line.size() + 1];
  wcscpy_s(szCmdline, command_line.size() + 1, command_line.c_str());
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFOW siStartInfo;
  BOOL bSuccess = FALSE;
  DWORD creation_flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW;

  // Set up members of the PROCESS_INFORMATION structure.
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

  // Set up members of the STARTUPINFOW structure.
  // This structure specifies the STDIN and STDOUT handles for redirection.

  ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
  siStartInfo.cb = sizeof(STARTUPINFOW);

  siStartInfo.hStdError = this->stream_.g_hChildStd_ERR_Wr;
  siStartInfo.hStdOutput = this->stream_.g_hChildStd_OUT_Wr;
  siStartInfo.hStdInput = this->stream_.g_hChildStd_IN_Rd;

  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  const wchar_t *cwd_arg = this->cwd_.empty() ? NULL : cwd_.c_str();

  // Create the child process.
  bSuccess = CreateProcessW(NULL,
                            szCmdline,    // command line
                            NULL,         // process security attributes
                            NULL,         // primary thread security attributes
                            TRUE,         // handles are inherited
                            creation_flags,	// creation flags
                            environment_string_table_ptr,  // use provided environment
                            cwd_arg,      // use provided current directory
                            &siStartInfo, // STARTUPINFOW pointer
                            &piProcInfo); // receives PROCESS_INFORMATION

  // If an error occurs, exit the application.
  if (!bSuccess) {
    DWORD errorMessageID = ::GetLastError();
    throw CalledProcessError("CreateProcess failed: " + util::get_last_error(errorMessageID), errorMessageID);
  }

  CloseHandle(piProcInfo.hThread);

  /*
    TODO: use common apis to close linux handles
  */

  this->process_handle_ = piProcInfo.hProcess;

  this->cleanup_future_ = std::async(std::launch::async, [this] {
    WaitForSingleObject(this->process_handle_, INFINITE);

    CloseHandle(this->stream_.g_hChildStd_ERR_Wr);
    CloseHandle(this->stream_.g_hChildStd_OUT_Wr);
    CloseHandle(this->stream_.g_hChildStd_IN_Rd);
  });

/*
  NOTE: In the linux version, there is a check to make sure that the process
        has been started. Here, we do nothing because CreateProcess will throw
        if we fail to create the process.
*/


#else

  int err_rd_pipe, err_wr_pipe;
  std::tie(err_rd_pipe, err_wr_pipe) = util::pipe_cloexec();

  if (shell_) {
    auto new_cmd = util::join(vargs_);
    vargs_.clear();
    vargs_.insert(vargs_.begin(), {"/bin/sh", "-c"});
    vargs_.push_back(new_cmd);
    populate_c_argv();
  }

  if (exe_name_.length()) {
    vargs_.insert(vargs_.begin(), exe_name_);
    populate_c_argv();
  }
  exe_name_ = vargs_[0];

  child_pid_ = fork();

  if (child_pid_ < 0) {
    subprocess_close(err_rd_pipe);
    subprocess_close(err_wr_pipe);
    throw OSError("fork failed", errno);
  }

  child_created_ = true;

  if (child_pid_ == 0)
  {
    // Close descriptors belonging to parent
    stream_.close_parent_fds();

    //Close the read end of the error pipe
    subprocess_close(err_rd_pipe);

    detail::Child chld(this, err_wr_pipe);
    chld.execute_child();
  }
  else
  {
    subprocess_close(err_wr_pipe);// close child side of pipe, else get stuck in read below

    stream_.close_child_fds();

    try {
      char err_buf[SP_MAX_ERR_BUF_SIZ] = {0,};

      FILE* err_fp = fdopen(err_rd_pipe, "r");
      if (!err_fp) {
          subprocess_close(err_rd_pipe);
          throw OSError("fdopen failed", errno);
      }
      int read_bytes = util::read_atmost_n(err_fp, err_buf, SP_MAX_ERR_BUF_SIZ);
      fclose(err_fp);

      if (read_bytes || strlen(err_buf)) {
        // Call waitpid to reap the child process
        // waitpid suspends the calling process until the
        // child terminates.
        int retcode = wait();

        // Throw whatever information we have about child failure
        throw CalledProcessError(err_buf, retcode);
      }
    } catch (std::exception& exp) {
      stream_.cleanup_fds();
      throw;
    }

  }
#endif
}

namespace detail {

  inline void ArgumentDeducer::set_option(executable&& exe) {
    popen_->exe_name_ = std::move(exe.arg_value);
  }

  inline void ArgumentDeducer::set_option(cwd&& cwdir) {
    popen_->cwd_ = std::move(cwdir.cwd_);
  }

  inline void ArgumentDeducer::set_option(bufsize&& bsiz) {
    popen_->stream_.bufsiz_ = bsiz.bufsiz;
  }

  inline void ArgumentDeducer::set_option(environment&& env) {
    popen_->env_ = std::move(env.env_);
  }

  inline void ArgumentDeducer::set_option(defer_spawn&& defer) {
    popen_->defer_process_start_ = defer.defer;
  }

  inline void ArgumentDeducer::set_option(shell&& sh) {
    popen_->shell_ = sh.shell_;
  }

  inline void ArgumentDeducer::set_option(session_leader&& sleader) {
    popen_->session_leader_ = sleader.leader_;
  }

  inline void ArgumentDeducer::set_option(input&& inp) {
    if (inp.rd_ch_ != -1) popen_->stream_.read_from_parent_ = inp.rd_ch_;
    if (inp.wr_ch_ != -1) popen_->stream_.write_to_child_ = inp.wr_ch_;
  }

  inline void ArgumentDeducer::set_option(output&& out) {
    if (out.wr_ch_ != -1) popen_->stream_.write_to_parent_ = out.wr_ch_;
    if (out.rd_ch_ != -1) popen_->stream_.read_from_child_ = out.rd_ch_;
  }

  inline void ArgumentDeducer::set_option(error&& err) {
    if (err.deferred_) {
      if (popen_->stream_.write_to_parent_) {
        popen_->stream_.err_write_ = popen_->stream_.write_to_parent_;
      } else {
        throw std::runtime_error("Set output before redirecting error to output");
      }
    }
    if (err.wr_ch_ != -1) popen_->stream_.err_write_ = err.wr_ch_;
    if (err.rd_ch_ != -1) popen_->stream_.err_read_ = err.rd_ch_;
  }

  inline void ArgumentDeducer::set_option(close_fds&& cfds) {
    popen_->close_fds_ = cfds.close_all;
  }

  inline void ArgumentDeducer::set_option(preexec_func&& prefunc) {
    popen_->preexec_fn_ = std::move(prefunc);
    popen_->has_preexec_fn_ = true;
  }


  inline void Child::execute_child() {
#ifndef __USING_WINDOWS__
    int sys_ret = -1;
    auto& stream = parent_->stream_;

    try {
      if (stream.write_to_parent_ == 0)
        stream.write_to_parent_ = dup(stream.write_to_parent_);

      if (stream.err_write_ == 0 || stream.err_write_ == 1)
        stream.err_write_ = dup(stream.err_write_);

      // Make the child owned descriptors as the
      // stdin, stdout and stderr for the child process
      auto _dup2_ = [](int fd, int to_fd) {
        if (fd == to_fd) {
          // dup2 syscall does not reset the
          // CLOEXEC flag if the descriptors
          // provided to it are same.
          // But, we need to reset the CLOEXEC
          // flag as the provided descriptors
          // are now going to be the standard
          // input, output and error
          util::set_clo_on_exec(fd, false);
        } else if(fd != -1) {
          int res = dup2(fd, to_fd);
          if (res == -1) throw OSError("dup2 failed", errno);
        }
      };

      // Create the standard streams
      _dup2_(stream.read_from_parent_, 0); // Input stream
      _dup2_(stream.write_to_parent_,  1); // Output stream
      _dup2_(stream.err_write_,        2); // Error stream

      // Close the duped descriptors
      if (stream.read_from_parent_ != -1 && stream.read_from_parent_ > 2)
        subprocess_close(stream.read_from_parent_);

      if (stream.write_to_parent_ != -1 && stream.write_to_parent_ > 2)
        subprocess_close(stream.write_to_parent_);

      if (stream.err_write_ != -1 && stream.err_write_ > 2)
        subprocess_close(stream.err_write_);

      // Close all the inherited fd's except the error write pipe
      if (parent_->close_fds_) {
        int max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd == -1) throw OSError("sysconf failed", errno);

        for (int i = 3; i < max_fd; i++) {
          if (i == err_wr_pipe_) continue;
          subprocess_close(i);
        }
      }

      // Change the working directory if provided
      if (parent_->cwd_.length()) {
        sys_ret = chdir(parent_->cwd_.c_str());
        if (sys_ret == -1) throw OSError("chdir failed", errno);
      }

      if (parent_->has_preexec_fn_) {
        parent_->preexec_fn_();
      }

      if (parent_->session_leader_) {
        sys_ret = setsid();
        if (sys_ret == -1) throw OSError("setsid failed", errno);
      }

      // Replace the current image with the executable
      if (parent_->env_.size()) {
        for (auto& kv : parent_->env_) {
          setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }
        sys_ret = execvp(parent_->exe_name_.c_str(), parent_->cargv_.data());
      } else {
        sys_ret = execvp(parent_->exe_name_.c_str(), parent_->cargv_.data());
      }

      if (sys_ret == -1) throw OSError("execve failed", errno);

    } catch (const OSError& exp) {
      // Just write the exception message
      // TODO: Give back stack trace ?
      std::string err_msg(exp.what());
      //ATTN: Can we do something on error here ?
      util::write_n(err_wr_pipe_, err_msg.c_str(), err_msg.length());
    }

    // Calling application would not get this
    // exit failure
    _exit (EXIT_FAILURE);
#endif
  }


  inline void Streams::setup_comm_channels()
  {
#ifdef __USING_WINDOWS__
    util::configure_pipe(&this->g_hChildStd_IN_Rd, &this->g_hChildStd_IN_Wr, &this->g_hChildStd_IN_Wr);
    this->input(util::file_from_handle(this->g_hChildStd_IN_Wr, "w"));
    this->write_to_child_ = subprocess_fileno(this->input());

    util::configure_pipe(&this->g_hChildStd_OUT_Rd, &this->g_hChildStd_OUT_Wr, &this->g_hChildStd_OUT_Rd);
    this->output(util::file_from_handle(this->g_hChildStd_OUT_Rd, "r"));
    this->read_from_child_ = subprocess_fileno(this->output());

    util::configure_pipe(&this->g_hChildStd_ERR_Rd, &this->g_hChildStd_ERR_Wr, &this->g_hChildStd_ERR_Rd);
    this->error(util::file_from_handle(this->g_hChildStd_ERR_Rd, "r"));
    this->err_read_ = subprocess_fileno(this->error());
#else

    if (write_to_child_ != -1)  input(fdopen(write_to_child_, "wb"));
    if (read_from_child_ != -1) output(fdopen(read_from_child_, "rb"));
    if (err_read_ != -1)        error(fdopen(err_read_, "rb"));

    auto handles = {input(), output(), error()};

    for (auto& h : handles) {
      if (h == nullptr) continue;
      switch (bufsiz_) {
      case 0:
        setvbuf(h, nullptr, _IONBF, BUFSIZ);
        break;
      case 1:
        setvbuf(h, nullptr, _IONBF, BUFSIZ);
        break;
      default:
        setvbuf(h, nullptr, _IOFBF, bufsiz_);
      };
    }
  #endif
  }

  inline int Communication::send(const char* msg, size_t length)
  {
    if (stream_->input() == nullptr) return -1;
    return std::fwrite(msg, sizeof(char), length, stream_->input());
  }

  inline int Communication::send(const std::vector<char>& msg)
  {
    return send(msg.data(), msg.size());
  }

  inline std::pair<OutBuffer, ErrBuffer>
  Communication::communicate(const char* msg, size_t length)
  {
    // Optimization from subprocess.py
    // If we are using one pipe, or no pipe
    // at all, using select() or threads is unnecessary.
    auto hndls = {stream_->input(), stream_->output(), stream_->error()};
    int count = std::count(std::begin(hndls), std::end(hndls), nullptr);
    const int len_conv = length;

    if (count >= 2) {
      OutBuffer obuf;
      ErrBuffer ebuf;
      if (stream_->input()) {
        if (msg) {
          int wbytes = std::fwrite(msg, sizeof(char), length, stream_->input());
          if (wbytes < len_conv) {
            if (errno != EPIPE && errno != EINVAL) {
              throw OSError("fwrite error", errno);
            }
          }
        }
        // Close the input stream
        stream_->input_.reset();
      } else if (stream_->output()) {
        // Read till EOF
        // ATTN: This could be blocking, if the process
        // at the other end screws up, we get screwed as well
        obuf.add_cap(out_buf_cap_);

        int rbytes = util::read_all(
                            stream_->output(),
                            obuf.buf);

        if (rbytes == -1) {
          throw OSError("read to obuf failed", errno);
        }

        obuf.length = rbytes;
        // Close the output stream
        stream_->output_.reset();

      } else if (stream_->error()) {
        // Same screwness applies here as well
        ebuf.add_cap(err_buf_cap_);

        int rbytes = util::read_atmost_n(
                                  stream_->error(),
                                  ebuf.buf.data(),
                                  ebuf.buf.size());

        if (rbytes == -1) {
          throw OSError("read to ebuf failed", errno);
        }

        ebuf.length = rbytes;
        // Close the error stream
        stream_->error_.reset();
      }
      return std::make_pair(std::move(obuf), std::move(ebuf));
    }

    return communicate_threaded(msg, length);
  }


  inline std::pair<OutBuffer, ErrBuffer>
  Communication::communicate_threaded(const char* msg, size_t length)
  {
    OutBuffer obuf;
    ErrBuffer ebuf;
    std::future<int> out_fut, err_fut;
    const int length_conv = length;

    if (stream_->output()) {
      obuf.add_cap(out_buf_cap_);

      out_fut = std::async(std::launch::async,
                          [&obuf, this] {
                            return util::read_all(this->stream_->output(), obuf.buf);
                          });
    }
    if (stream_->error()) {
      ebuf.add_cap(err_buf_cap_);

      err_fut = std::async(std::launch::async,
                          [&ebuf, this] {
                            return util::read_all(this->stream_->error(), ebuf.buf);
                          });
    }
    if (stream_->input()) {
      if (msg) {
        int wbytes = std::fwrite(msg, sizeof(char), length, stream_->input());
        if (wbytes < length_conv) {
          if (errno != EPIPE && errno != EINVAL) {
            throw OSError("fwrite error", errno);
          }
        }
      }
      stream_->input_.reset();
    }

    if (out_fut.valid()) {
      int res = out_fut.get();
      if (res != -1) obuf.length = res;
      else obuf.length = 0;
    }
    if (err_fut.valid()) {
      int res = err_fut.get();
      if (res != -1) ebuf.length = res;
      else ebuf.length = 0;
    }

    return std::make_pair(std::move(obuf), std::move(ebuf));
  }

} // end namespace detail



// Convenience Functions
//
//
namespace detail
{
  template<typename F, typename... Args>
  OutBuffer check_output_impl(F& farg, Args&&... args)
  {
    static_assert(!detail::has_type<output, detail::param_pack<Args...>>::value, "output not allowed in args");
    auto p = Popen(std::forward<F>(farg), std::forward<Args>(args)..., output{PIPE});
    auto res = p.communicate();
    auto retcode = p.retcode();
    if (retcode > 0) {
      throw CalledProcessError("Command failed : Non zero retcode", retcode);
    }
    return std::move(res.first);
  }

  template<typename F, typename... Args>
  int call_impl(F& farg, Args&&... args)
  {
    return Popen(std::forward<F>(farg), std::forward<Args>(args)...).wait();
  }

  static inline void pipeline_impl(std::vector<Popen>& cmds)
  {
    /* EMPTY IMPL */
  }

  template<typename... Args>
  static inline void pipeline_impl(std::vector<Popen>& cmds,
                                   const std::string& cmd,
                                   Args&&... args)
  {
    if (cmds.size() == 0) {
      cmds.emplace_back(cmd, output{PIPE}, defer_spawn{true});
    } else {
      cmds.emplace_back(cmd, input{cmds.back().output()}, output{PIPE}, defer_spawn{true});
    }

    pipeline_impl(cmds, std::forward<Args>(args)...);
  }

}

/*-----------------------------------------------------------
 *        CONVENIENCE FUNCTIONS
 *-----------------------------------------------------------
 */


/*!
 * Run the command with arguments and wait for it to complete.
 * The parameters passed to the argument are exactly the same
 * one would use for Popen constructors.
 */
template<typename... Args>
int call(std::initializer_list<const char*> plist, Args&&... args)
{
  return (detail::call_impl(plist, std::forward<Args>(args)...));
}

template<typename... Args>
int call(const std::string& arg, Args&&... args)
{
  return (detail::call_impl(arg, std::forward<Args>(args)...));
}

template <typename... Args>
int call(std::vector<std::string> plist, Args &&... args)
{
  return (detail::call_impl(plist, std::forward<Args>(args)...));
}


/*!
 * Run the command with arguments and wait for it to complete.
 * If the exit code was non-zero it raises a CalledProcessError.
 * The arguments are the same as for the Popen constructor.
 */
template <typename... Args>
OutBuffer check_output(std::initializer_list<const char*> plist, Args&&... args)
{
  return (detail::check_output_impl(plist, std::forward<Args>(args)...));
}

template <typename... Args>
OutBuffer check_output(const std::string& arg, Args&&... args)
{
  return (detail::check_output_impl(arg, std::forward<Args>(args)...));
}

template <typename... Args>
OutBuffer check_output(std::vector<std::string> plist, Args &&... args)
{
  return (detail::check_output_impl(plist, std::forward<Args>(args)...));
}


/*!
 * An easy way to pipeline easy commands.
 * Provide the commands that needs to be pipelined in the order they
 * would appear in a regular command.
 * It would wait for the last command provided in the pipeline
 * to finish and then return the OutBuffer.
 */
template<typename... Args>
// Args expected to be flat commands using string instead of initializer_list
OutBuffer pipeline(Args&&... args)
{
  std::vector<Popen> pcmds;
  detail::pipeline_impl(pcmds, std::forward<Args>(args)...);

  for (auto& p : pcmds) p.start_process();
  return (pcmds.back().communicate().first);
}

}

#endif // SUBPROCESS_HPP
