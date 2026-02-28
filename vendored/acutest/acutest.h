/*
 * Acutest -- Another C/C++ Unit Test facility
 * <https://github.com/mity/acutest>
 *
 * Copyright 2013-2023 Martin Mitáš
 * Copyright 2019 Garrett D'Amore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef ACUTEST_H
#define ACUTEST_H


/* Try to auto-detect whether we need to disable C++ exception handling.
 * If the detection fails, you may always define TEST_NO_EXCEPTIONS before
 * including "acutest.h" manually. */
#ifdef __cplusplus
    #if (__cplusplus >= 199711L && !defined __cpp_exceptions)  ||            \
        ((defined(__GNUC__) || defined(__clang__)) && !defined __EXCEPTIONS)
        #ifndef TEST_NO_EXCEPTIONS
            #define TEST_NO_EXCEPTIONS
        #endif
    #endif
#endif


/************************
 *** Public interface ***
 ************************/

/* By default, "acutest.h" provides the main program entry point (function
 * main()). However, if the test suite is composed of multiple source files
 * which include "acutest.h", then this causes a problem of multiple main()
 * definitions. To avoid this problem, #define macro TEST_NO_MAIN in all
 * compilation units but one.
 */

/* Macro to specify list of unit tests in the suite.
 * The unit test implementation MUST provide list of unit tests it implements
 * with this macro:
 *
 *   TEST_LIST = {
 *       { "test1_name", test1_func_ptr },
 *       { "test2_name", test2_func_ptr },
 *       ...
 *       { NULL, NULL }     // zeroed record marking the end of the list
 *   };
 *
 * The list specifies names of each test (must be unique) and pointer to
 * a function implementing it. The function does not take any arguments
 * and has no return values, i.e. every test function has to be compatible
 * with this prototype:
 *
 *   void test_func(void);
 *
 * Note the list has to be ended with a zeroed record.
 */
#define TEST_LIST               const struct acutest_test_ acutest_list_[]


/* Macros for testing whether an unit test succeeds or fails. These macros
 * can be used arbitrarily in functions implementing the unit tests.
 *
 * If any condition fails throughout execution of a test, the test fails.
 *
 * TEST_CHECK takes only one argument (the condition), TEST_CHECK_ allows
 * also to specify an error message to print out if the condition fails.
 * (It expects printf-like format string and its parameters). The macros
 * return non-zero (condition passes) or 0 (condition fails).
 *
 * That can be useful when more conditions should be checked only if some
 * preceding condition passes, as illustrated in this code snippet:
 *
 *   SomeStruct* ptr = allocate_some_struct();
 *   if(TEST_CHECK(ptr != NULL)) {
 *       TEST_CHECK(ptr->member1 < 100);
 *       TEST_CHECK(ptr->member2 > 200);
 *   }
 */
#define TEST_CHECK_(cond,...)                                                  \
    acutest_check_(!!(cond), __FILE__, __LINE__, __VA_ARGS__)
#define TEST_CHECK(cond)                                                       \
    acutest_check_(!!(cond), __FILE__, __LINE__, "%s", #cond)


/* These macros are the same as TEST_CHECK_ and TEST_CHECK except that if the
 * condition fails, the currently executed unit test is immediately aborted.
 *
 * That is done either by calling abort() if the unit test is executed as a
 * child process; or via longjmp() if the unit test is executed within the
 * main Acutest process.
 *
 * As a side effect of such abortion, your unit tests may cause memory leaks,
 * unflushed file descriptors, and other phenomena caused by the abortion.
 *
 * Therefore you should not use these as a general replacement for TEST_CHECK.
 * Use it with some caution, especially if your test causes some other side
 * effects to the outside world (e.g. communicating with some server, inserting
 * into a database etc.).
 */
#define TEST_ASSERT_(cond,...)                                                 \
    do {                                                                       \
        if(!acutest_check_(!!(cond), __FILE__, __LINE__, __VA_ARGS__))         \
            acutest_abort_();                                                  \
    } while(0)
#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if(!acutest_check_(!!(cond), __FILE__, __LINE__, "%s", #cond))         \
            acutest_abort_();                                                  \
    } while(0)


#ifdef __cplusplus
#ifndef TEST_NO_EXCEPTIONS
/* Macros to verify that the code (the 1st argument) throws exception of given
 * type (the 2nd argument). (Note these macros are only available in C++.)
 *
 * TEST_EXCEPTION_ is like TEST_EXCEPTION but accepts custom printf-like
 * message.
 *
 * For example:
 *
 *   TEST_EXCEPTION(function_that_throw(), ExpectedExceptionType);
 *
 * If the function_that_throw() throws ExpectedExceptionType, the check passes.
 * If the function throws anything incompatible with ExpectedExceptionType
 * (or if it does not thrown an exception at all), the check fails.
 */
#define TEST_EXCEPTION(code, exctype)                                          \
    do {                                                                       \
        bool exc_ok_ = false;                                                  \
        const char *msg_ = NULL;                                               \
        try {                                                                  \
            code;                                                              \
            msg_ = "No exception thrown.";                                     \
        } catch(exctype const&) {                                              \
            exc_ok_= true;                                                     \
        } catch(...) {                                                         \
            msg_ = "Unexpected exception thrown.";                             \
        }                                                                      \
        acutest_check_(exc_ok_, __FILE__, __LINE__, #code " throws " #exctype);\
        if(msg_ != NULL)                                                       \
            acutest_message_("%s", msg_);                                      \
    } while(0)
#define TEST_EXCEPTION_(code, exctype, ...)                                    \
    do {                                                                       \
        bool exc_ok_ = false;                                                  \
        const char *msg_ = NULL;                                               \
        try {                                                                  \
            code;                                                              \
            msg_ = "No exception thrown.";                                     \
        } catch(exctype const&) {                                              \
            exc_ok_= true;                                                     \
        } catch(...) {                                                         \
            msg_ = "Unexpected exception thrown.";                             \
        }                                                                      \
        acutest_check_(exc_ok_, __FILE__, __LINE__, __VA_ARGS__);              \
        if(msg_ != NULL)                                                       \
            acutest_message_("%s", msg_);                                      \
    } while(0)
#endif  /* #ifndef TEST_NO_EXCEPTIONS */
#endif  /* #ifdef __cplusplus */


/* Sometimes it is useful to split execution of more complex unit tests to some
 * smaller parts and associate those parts with some names.
 *
 * This is especially handy if the given unit test is implemented as a loop
 * over some vector of multiple testing inputs. Using these macros allow to use
 * sort of subtitle for each iteration of the loop (e.g. outputting the input
 * itself or a name associated to it), so that if any TEST_CHECK condition
 * fails in the loop, it can be easily seen which iteration triggers the
 * failure, without the need to manually output the iteration-specific data in
 * every single TEST_CHECK inside the loop body.
 *
 * TEST_CASE allows to specify only single string as the name of the case,
 * TEST_CASE_ provides all the power of printf-like string formatting.
 *
 * Note that the test cases cannot be nested. Starting a new test case ends
 * implicitly the previous one. To end the test case explicitly (e.g. to end
 * the last test case after exiting the loop), you may use TEST_CASE(NULL).
 */
#define TEST_CASE_(...)         acutest_case_(__VA_ARGS__)
#define TEST_CASE(name)         acutest_case_("%s", name)


/* Maximal output per TEST_CASE call. Longer messages are cut.
 * You may define another limit prior including "acutest.h"
 */
#ifndef TEST_CASE_MAXSIZE
    #define TEST_CASE_MAXSIZE    64
#endif


/* printf-like macro for outputting an extra information about a failure.
 *
 * Intended use is to output some computed output versus the expected value,
 * e.g. like this:
 *
 *   if(!TEST_CHECK(produced == expected)) {
 *       TEST_MSG("Expected: %d", expected);
 *       TEST_MSG("Produced: %d", produced);
 *   }
 *
 * Note the message is only written down if the most recent use of any checking
 * macro (like e.g. TEST_CHECK or TEST_EXCEPTION) in the current test failed.
 * This means the above is equivalent to just this:
 *
 *   TEST_CHECK(produced == expected);
 *   TEST_MSG("Expected: %d", expected);
 *   TEST_MSG("Produced: %d", produced);
 *
 * The macro can deal with multi-line output fairly well. It also automatically
 * adds a final new-line if there is none present.
 */
#define TEST_MSG(...)           acutest_message_(__VA_ARGS__)


/* Maximal output per TEST_MSG call. Longer messages are cut.
 * You may define another limit prior including "acutest.h"
 */
#ifndef TEST_MSG_MAXSIZE
    #define TEST_MSG_MAXSIZE    1024
#endif


/* Macro for dumping a block of memory.
 *
 * Its intended use is very similar to what TEST_MSG is for, but instead of
 * generating any printf-like message, this is for dumping raw block of a
 * memory in a hexadecimal form:
 *
 *   TEST_CHECK(size_produced == size_expected &&
 *              memcmp(addr_produced, addr_expected, size_produced) == 0);
 *   TEST_DUMP("Expected:", addr_expected, size_expected);
 *   TEST_DUMP("Produced:", addr_produced, size_produced);
 */
#define TEST_DUMP(title, addr, size)    acutest_dump_(title, addr, size)

/* Maximal output per TEST_DUMP call (in bytes to dump). Longer blocks are cut.
 * You may define another limit prior including "acutest.h"
 */
#ifndef TEST_DUMP_MAXSIZE
    #define TEST_DUMP_MAXSIZE   1024
#endif


/* Macros for marking the test as SKIPPED.
 * Note it can only be used at the beginning of a test, before any other
 * checking.
 *
 * Once used, the best practice is to return from the test routine as soon
 * as possible.
 */
#define TEST_SKIP(...)         acutest_skip_(__FILE__, __LINE__, __VA_ARGS__)


/* Common test initialisation/clean-up
 *
 * In some test suites, it may be needed to perform some sort of the same
 * initialization and/or clean-up in all the tests.
 *
 * Such test suites may use macros TEST_INIT and/or TEST_FINI prior including
 * this header. The expansion of the macro is then used as a body of helper
 * function called just before executing every single (TEST_INIT) or just after
 * it ends (TEST_FINI).
 *
 * Examples of various ways how to use the macro TEST_INIT:
 *
 *   #define TEST_INIT      my_init_func();
 *   #define TEST_INIT      my_init_func()      // Works even without the semicolon
 *   #define TEST_INIT      setlocale(LC_ALL, NULL);
 *   #define TEST_INIT      { setlocale(LC_ALL, NULL); my_init_func(); }
 *
 * TEST_FINI is to be used in the same way.
 */


/**********************
 *** Implementation ***
 **********************/

/* The unit test files should not rely on anything below. */

#include <stdlib.h>

/* Enable the use of the non-standard keyword __attribute__ to silence warnings under some compilers */
#if defined(__GNUC__) || defined(__clang__)
    #define ACUTEST_ATTRIBUTE_(attr)    __attribute__((attr))
#else
    #define ACUTEST_ATTRIBUTE_(attr)
#endif

#ifdef __cplusplus
    extern "C" {
#endif

enum acutest_state_ {
    ACUTEST_STATE_INITIAL = -4,
    ACUTEST_STATE_SELECTED = -3,
    ACUTEST_STATE_NEEDTORUN = -2,

    /* By the end all tests should be in one of the following: */
    ACUTEST_STATE_EXCLUDED = -1,
    ACUTEST_STATE_SUCCESS = 0,
    ACUTEST_STATE_FAILED = 1,
    ACUTEST_STATE_SKIPPED = 2
};

int acutest_check_(int cond, const char* file, int line, const char* fmt, ...);
void acutest_case_(const char* fmt, ...);
void acutest_message_(const char* fmt, ...);
void acutest_dump_(const char* title, const void* addr, size_t size);
void acutest_abort_(void) ACUTEST_ATTRIBUTE_(noreturn);
#ifdef __cplusplus
    }  /* extern "C" */
#endif

#ifndef TEST_NO_MAIN

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__)
    #define ACUTEST_UNIX_       1
    #include <errno.h>
    #include <libgen.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <time.h>

    #if defined CLOCK_PROCESS_CPUTIME_ID  &&  defined CLOCK_MONOTONIC
        #define ACUTEST_HAS_POSIX_TIMER_    1
    #endif
#endif

#if defined(_gnu_linux_) || defined(__linux__)
    #define ACUTEST_LINUX_      1
    #include <fcntl.h>
    #include <sys/stat.h>
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    #define ACUTEST_WIN_        1
    #include <windows.h>
    #include <io.h>
#endif

#if defined(__APPLE__)
    #define ACUTEST_MACOS_
    #include <assert.h>
    #include <stdbool.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/sysctl.h>
#endif

#ifdef __cplusplus
#ifndef TEST_NO_EXCEPTIONS
    #include <exception>
#endif
#endif

#ifdef __has_include
    #if __has_include(<valgrind.h>)
        #include <valgrind.h>
    #endif
#endif

/* Note our global private identifiers end with '_' to mitigate risk of clash
 * with the unit tests implementation. */

#ifdef __cplusplus
    extern "C" {
#endif

#ifdef _MSC_VER
    /* In the multi-platform code like ours, we cannot use the non-standard
     * "safe" functions from Microsoft C lib like e.g. sprintf_s() instead of
     * standard sprintf(). Hence, lets disable the warning C4996. */
    #pragma warning(push)
    #pragma warning(disable: 4996)
#endif


struct acutest_test_ {
    const char* name;
    void (*func)(void);
};

struct acutest_test_data_ {
    enum acutest_state_ state;
    double duration;
};


extern const struct acutest_test_ acutest_list_[];


static char* acutest_argv0_ = NULL;
static int acutest_list_size_ = 0;
static struct acutest_test_data_* acutest_test_data_ = NULL;
static int acutest_no_exec_ = -1;
static int acutest_no_summary_ = 0;
static int acutest_tap_ = 0;
static int acutest_exclude_mode_ = 0;
static int acutest_worker_ = 0;
static int acutest_worker_index_ = 0;
static int acutest_cond_failed_ = 0;
static FILE *acutest_xml_output_ = NULL;

static const struct acutest_test_* acutest_current_test_ = NULL;
static int acutest_current_index_ = 0;
static char acutest_case_name_[TEST_CASE_MAXSIZE] = "";
static int acutest_test_check_count_ = 0;
static int acutest_test_skip_count_ = 0;
static char acutest_test_skip_reason_[256] = "";
static int acutest_test_already_logged_ = 0;
static int acutest_case_already_logged_ = 0;
static int acutest_verbose_level_ = 2;
static int acutest_test_failures_ = 0;
static int acutest_colorize_ = 0;
static int acutest_timer_ = 0;

static int acutest_abort_has_jmp_buf_ = 0;
static jmp_buf acutest_abort_jmp_buf_;

static int
acutest_count_(enum acutest_state_ state)
{
    int i, n;

    for(i = 0, n = 0; i < acutest_list_size_; i++) {
        if(acutest_test_data_[i].state == state)
            n++;
    }

    return n;
}

static void
acutest_cleanup_(void)
{
    free((void*) acutest_test_data_);
}

static void ACUTEST_ATTRIBUTE_(noreturn)
acutest_exit_(int exit_code)
{
    acutest_cleanup_();
    exit(exit_code);
}


#if defined ACUTEST_WIN_
    typedef LARGE_INTEGER acutest_timer_type_;
    static LARGE_INTEGER acutest_timer_freq_;
    static acutest_timer_type_ acutest_timer_start_;
    static acutest_timer_type_ acutest_timer_end_;

    static void
    acutest_timer_init_(void)
    {
        QueryPerformanceFrequency(&acutest_timer_freq_);
    }

    static void
    acutest_timer_get_time_(LARGE_INTEGER* ts)
    {
        QueryPerformanceCounter(ts);
    }

    static double
    acutest_timer_diff_(LARGE_INTEGER start, LARGE_INTEGER end)
    {
        double duration = (double)(end.QuadPart - start.QuadPart);
        duration /= (double)acutest_timer_freq_.QuadPart;
        return duration;
    }

    static void
    acutest_timer_print_diff_(void)
    {
        printf("%.6lf secs", acutest_timer_diff_(acutest_timer_start_, acutest_timer_end_));
    }
#elif defined ACUTEST_HAS_POSIX_TIMER_
    static clockid_t acutest_timer_id_;
    typedef struct timespec acutest_timer_type_;
    static acutest_timer_type_ acutest_timer_start_;
    static acutest_timer_type_ acutest_timer_end_;

    static void
    acutest_timer_init_(void)
    {
        if(acutest_timer_ == 1)
            acutest_timer_id_ = CLOCK_MONOTONIC;
        else if(acutest_timer_ == 2)
            acutest_timer_id_ = CLOCK_PROCESS_CPUTIME_ID;
    }

    static void
    acutest_timer_get_time_(struct timespec* ts)
    {
        clock_gettime(acutest_timer_id_, ts);
    }

    static double
    acutest_timer_diff_(struct timespec start, struct timespec end)
    {
        return (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    }

    static void
    acutest_timer_print_diff_(void)
    {
        printf("%.6lf secs",
            acutest_timer_diff_(acutest_timer_start_, acutest_timer_end_));
    }
#else
    typedef int acutest_timer_type_;
    static acutest_timer_type_ acutest_timer_start_;
    static acutest_timer_type_ acutest_timer_end_;

    void
    acutest_timer_init_(void)
    {}

    static void
    acutest_timer_get_time_(int* ts)
    {
        (void) ts;
    }

    static double
    acutest_timer_diff_(int start, int end)
    {
        (void) start;
        (void) end;
        return 0.0;
    }

    static void
    acutest_timer_print_diff_(void)
    {}
#endif

#define ACUTEST_COLOR_DEFAULT_              0
#define ACUTEST_COLOR_RED_                  1
#define ACUTEST_COLOR_GREEN_                2
#define ACUTEST_COLOR_YELLOW_               3
#define ACUTEST_COLOR_DEFAULT_INTENSIVE_    10
#define ACUTEST_COLOR_RED_INTENSIVE_        11
#define ACUTEST_COLOR_GREEN_INTENSIVE_      12
#define ACUTEST_COLOR_YELLOW_INTENSIVE_     13

static int ACUTEST_ATTRIBUTE_(format (printf, 2, 3))
acutest_colored_printf_(int color, const char* fmt, ...)
{
    va_list args;
    char buffer[256];
    int n;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer)-1] = '\0';

    if(!acutest_colorize_) {
        return printf("%s", buffer);
    }

#if defined ACUTEST_UNIX_
    {
        const char* col_str;
        switch(color) {
            case ACUTEST_COLOR_RED_:                col_str = "\033[0;31m"; break;
            case ACUTEST_COLOR_GREEN_:              col_str = "\033[0;32m"; break;
            case ACUTEST_COLOR_YELLOW_:             col_str = "\033[0;33m"; break;
            case ACUTEST_COLOR_RED_INTENSIVE_:      col_str = "\033[1;31m"; break;
            case ACUTEST_COLOR_GREEN_INTENSIVE_:    col_str = "\033[1;32m"; break;
            case ACUTEST_COLOR_YELLOW_INTENSIVE_:   col_str = "\033[1;33m"; break;
            case ACUTEST_COLOR_DEFAULT_INTENSIVE_:  col_str = "\033[1m"; break;
            default:                                col_str = "\033[0m"; break;
        }
        printf("%s", col_str);
        n = printf("%s", buffer);
        printf("\033[0m");
        return n;
    }
#elif defined ACUTEST_WIN_
    {
        HANDLE h;
        CONSOLE_SCREEN_BUFFER_INFO info;
        WORD attr;

        h = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(h, &info);

        switch(color) {
            case ACUTEST_COLOR_RED_:                attr = FOREGROUND_RED; break;
            case ACUTEST_COLOR_GREEN_:              attr = FOREGROUND_GREEN; break;
            case ACUTEST_COLOR_YELLOW_:             attr = FOREGROUND_RED | FOREGROUND_GREEN; break;
            case ACUTEST_COLOR_RED_INTENSIVE_:      attr = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            case ACUTEST_COLOR_GREEN_INTENSIVE_:    attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            case ACUTEST_COLOR_DEFAULT_INTENSIVE_:  attr = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            case ACUTEST_COLOR_YELLOW_INTENSIVE_:   attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            default:                                attr = 0; break;
        }
        if(attr != 0)
            SetConsoleTextAttribute(h, attr);
        n = printf("%s", buffer);
        SetConsoleTextAttribute(h, info.wAttributes);
        return n;
    }
#else
    n = printf("%s", buffer);
    return n;
#endif
}

static const char*
acutest_basename_(const char* path)
{
    const char* name;

    name = strrchr(path, '/');
    if(name != NULL)
        name++;
    else
        name = path;

#ifdef ACUTEST_WIN_
    {
        const char* alt_name;

        alt_name = strrchr(path, '\\');
        if(alt_name != NULL)
            alt_name++;
        else
            alt_name = path;

        if(alt_name > name)
            name = alt_name;
    }
#endif

    return name;
}

static void
acutest_begin_test_line_(const struct acutest_test_* test)
{
    if(!acutest_tap_) {
        if(acutest_verbose_level_ >= 3) {
            acutest_colored_printf_(ACUTEST_COLOR_DEFAULT_INTENSIVE_, "Test %s:\n", test->name);
            acutest_test_already_logged_++;
        } else if(acutest_verbose_level_ >= 1) {
            int n;
            char spaces[48];

            n = acutest_colored_printf_(ACUTEST_COLOR_DEFAULT_INTENSIVE_, "Test %s... ", test->name);
            memset(spaces, ' ', sizeof(spaces));
            if(n < (int) sizeof(spaces))
                printf("%.*s", (int) sizeof(spaces) - n, spaces);
        } else {
            acutest_test_already_logged_ = 1;
        }
    }
}

static void
acutest_finish_test_line_(enum acutest_state_ state)
{
    if(acutest_tap_) {
        printf("%s %d - %s%s\n",
                (state == ACUTEST_STATE_SUCCESS || state == ACUTEST_STATE_SKIPPED) ? "ok" : "not ok",
                acutest_current_index_ + 1,
                acutest_current_test_->name,
                (state == ACUTEST_STATE_SKIPPED) ? " # SKIP" : "");

        if(state == ACUTEST_STATE_SUCCESS  &&  acutest_timer_) {
            printf("# Duration: ");
            acutest_timer_print_diff_();
            printf("\n");
        }
    } else {
        int color;
        const char* str;

        switch(state) {
            case ACUTEST_STATE_SUCCESS: color = ACUTEST_COLOR_GREEN_INTENSIVE_; str = "OK"; break;
            case ACUTEST_STATE_SKIPPED: color = ACUTEST_COLOR_YELLOW_INTENSIVE_; str = "SKIPPED"; break;
            case ACUTEST_STATE_FAILED:  /* Fall through. */
            default:                    color = ACUTEST_COLOR_RED_INTENSIVE_; str = "FAILED"; break;
        }

        printf("[ ");
        acutest_colored_printf_(color, "%s", str);
        printf(" ]");

        if(state == ACUTEST_STATE_SUCCESS  &&  acutest_timer_) {
            printf("  ");
            acutest_timer_print_diff_();
        }

        printf("\n");
    }
}

static void
acutest_line_indent_(int level)
{
    static const char spaces[] = "                ";
    int n = level * 2;

    if(acutest_tap_  &&  n > 0) {
        n--;
        printf("#");
    }

    while(n > 16) {
        printf("%s", spaces);
        n -= 16;
    }
    printf("%.*s", n, spaces);
}

void ACUTEST_ATTRIBUTE_(format (printf, 3, 4))
acutest_skip_(const char* file, int line, const char* fmt, ...)
{
    va_list args;
    size_t reason_len;

    va_start(args, fmt);
    vsnprintf(acutest_test_skip_reason_, sizeof(acutest_test_skip_reason_), fmt, args);
    va_end(args);
    acutest_test_skip_reason_[sizeof(acutest_test_skip_reason_)-1] = '\0';

    /* Remove final dot, if provided; that collides with our other logic. */
    reason_len = strlen(acutest_test_skip_reason_);
    if(acutest_test_skip_reason_[reason_len-1] == '.')
        acutest_test_skip_reason_[reason_len-1] = '\0';

    if(acutest_test_check_count_ > 0) {
        acutest_check_(0, file, line, "Cannot skip, already performed some checks");
        return;
    }

    if(acutest_verbose_level_ >= 2) {
        const char *result_str = "skipped";
        int result_color = ACUTEST_COLOR_YELLOW_;

        if(!acutest_test_already_logged_  &&  acutest_current_test_ != NULL)
            acutest_finish_test_line_(ACUTEST_STATE_SKIPPED);
        acutest_test_already_logged_++;

        acutest_line_indent_(1);

        if(file != NULL) {
            file = acutest_basename_(file);
            printf("%s:%d: ", file, line);
        }

        printf("%s... ", acutest_test_skip_reason_);
        acutest_colored_printf_(result_color, "%s", result_str);
        printf("\n");
        acutest_test_already_logged_++;
    }

    acutest_test_skip_count_++;
}

int ACUTEST_ATTRIBUTE_(format (printf, 4, 5))
acutest_check_(int cond, const char* file, int line, const char* fmt, ...)
{
    const char *result_str;
    int result_color;
    int verbose_level;

    if(acutest_test_skip_count_) {
        /* We've skipped the test. We shouldn't be here: The test implementation
         * should have already return before. So lets suppress the following
         * output. */
        cond = 1;
        goto skip_check;
    }

    if(cond) {
        result_str = "ok";
        result_color = ACUTEST_COLOR_GREEN_;
        verbose_level = 3;
    } else {
        if(!acutest_test_already_logged_  &&  acutest_current_test_ != NULL)
            acutest_finish_test_line_(ACUTEST_STATE_FAILED);

        acutest_test_failures_++;
        acutest_test_already_logged_++;

        result_str = "failed";
        result_color = ACUTEST_COLOR_RED_;
        verbose_level = 2;
    }

    if(acutest_verbose_level_ >= verbose_level) {
        va_list args;

        if(!acutest_case_already_logged_  &&  acutest_case_name_[0]) {
            acutest_line_indent_(1);
            acutest_colored_printf_(ACUTEST_COLOR_DEFAULT_INTENSIVE_, "Case %s:\n", acutest_case_name_);
            acutest_test_already_logged_++;
            acutest_case_already_logged_++;
        }

        acutest_line_indent_(acutest_case_name_[0] ? 2 : 1);
        if(file != NULL) {
            file = acutest_basename_(file);
            printf("%s:%d: ", file, line);
        }

        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        printf("... ");
        acutest_colored_printf_(result_color, "%s", result_str);
        printf("\n");
        acutest_test_already_logged_++;
    }

    acutest_test_check_count_++;

skip_check:
    acutest_cond_failed_ = (cond == 0);
    return !acutest_cond_failed_;
}

void ACUTEST_ATTRIBUTE_(format (printf, 1, 2))
acutest_case_(const char* fmt, ...)
{
    va_list args;

    if(acutest_verbose_level_ < 2)
        return;

    if(acutest_case_name_[0]) {
        acutest_case_already_logged_ = 0;
        acutest_case_name_[0] = '\0';
    }

    if(fmt == NULL)
        return;

    va_start(args, fmt);
    vsnprintf(acutest_case_name_, sizeof(acutest_case_name_) - 1, fmt, args);
    va_end(args);
    acutest_case_name_[sizeof(acutest_case_name_) - 1] = '\0';

    if(acutest_verbose_level_ >= 3) {
        acutest_line_indent_(1);
        acutest_colored_printf_(ACUTEST_COLOR_DEFAULT_INTENSIVE_, "Case %s:\n", acutest_case_name_);
        acutest_test_already_logged_++;
        acutest_case_already_logged_++;
    }
}

void ACUTEST_ATTRIBUTE_(format (printf, 1, 2))
acutest_message_(const char* fmt, ...)
{
    char buffer[TEST_MSG_MAXSIZE];
    char* line_beg;
    char* line_end;
    va_list args;

    if(acutest_verbose_level_ < 2)
        return;

    /* We allow extra message only when something is already wrong in the
     * current test. */
    if(acutest_current_test_ == NULL  ||  !acutest_cond_failed_)
        return;

    va_start(args, fmt);
    vsnprintf(buffer, TEST_MSG_MAXSIZE, fmt, args);
    va_end(args);
    buffer[TEST_MSG_MAXSIZE-1] = '\0';

    line_beg = buffer;
    while(1) {
        line_end = strchr(line_beg, '\n');
        if(line_end == NULL)
            break;
        acutest_line_indent_(acutest_case_name_[0] ? 3 : 2);
        printf("%.*s\n", (int)(line_end - line_beg), line_beg);
        line_beg = line_end + 1;
    }
    if(line_beg[0] != '\0') {
        acutest_line_indent_(acutest_case_name_[0] ? 3 : 2);
        printf("%s\n", line_beg);
    }
}

void
acutest_dump_(const char* title, const void* addr, size_t size)
{
    static const size_t BYTES_PER_LINE = 16;
    size_t line_beg;
    size_t truncate = 0;

    if(acutest_verbose_level_ < 2)
        return;

    /* We allow extra message only when something is already wrong in the
     * current test. */
    if(acutest_current_test_ == NULL  ||  !acutest_cond_failed_)
        return;

    if(size > TEST_DUMP_MAXSIZE) {
        truncate = size - TEST_DUMP_MAXSIZE;
        size = TEST_DUMP_MAXSIZE;
    }

    acutest_line_indent_(acutest_case_name_[0] ? 3 : 2);
    printf((title[strlen(title)-1] == ':') ? "%s\n" : "%s:\n", title);

    for(line_beg = 0; line_beg < size; line_beg += BYTES_PER_LINE) {
        size_t line_end = line_beg + BYTES_PER_LINE;
        size_t off;

        acutest_line_indent_(acutest_case_name_[0] ? 4 : 3);
        printf("%08lx: ", (unsigned long)line_beg);
        for(off = line_beg; off < line_end; off++) {
            if(off < size)
                printf(" %02x", ((const unsigned char*)addr)[off]);
            else
                printf("   ");
        }

        printf("  ");
        for(off = line_beg; off < line_end; off++) {
            unsigned char byte = ((const unsigned char*)addr)[off];
            if(off < size)
                printf("%c", (iscntrl(byte) ? '.' : byte));
            else
                break;
        }

        printf("\n");
    }

    if(truncate > 0) {
        acutest_line_indent_(acutest_case_name_[0] ? 4 : 3);
        printf("           ... (and more %u bytes)\n", (unsigned) truncate);
    }
}

/* This is called just before each test */
static void
acutest_init_(const char *test_name)
{
#ifdef TEST_INIT
    TEST_INIT
    ; /* Allow for a single unterminated function call */
#endif

    /* Suppress any warnings about unused variable. */
    (void) test_name;
}

/* This is called after each test */
static void
acutest_fini_(const char *test_name)
{
#ifdef TEST_FINI
    TEST_FINI
    ; /* Allow for a single unterminated function call */
#endif

    /* Suppress any warnings about unused variable. */
    (void) test_name;
}

void
acutest_abort_(void)
{
    if(acutest_abort_has_jmp_buf_) {
        longjmp(acutest_abort_jmp_buf_, 1);
    } else {
        if(acutest_current_test_ != NULL)
            acutest_fini_(acutest_current_test_->name);
        fflush(stdout);
        fflush(stderr);
        acutest_exit_(ACUTEST_STATE_FAILED);
    }
}

static void
acutest_list_names_(void)
{
    const struct acutest_test_* test;

    printf("Unit tests:\n");
    for(test = &acutest_list_[0]; test->func != NULL; test++)
        printf("  %s\n", test->name);
}

static int
acutest_name_contains_word_(const char* name, const char* pattern)
{
    static const char word_delim[] = " \t-_/.,:;";
    const char* substr;
    size_t pattern_len;

    pattern_len = strlen(pattern);

    substr = strstr(name, pattern);
    while(substr != NULL) {
        int starts_on_word_boundary = (substr == name || strchr(word_delim, substr[-1]) != NULL);
        int ends_on_word_boundary = (substr[pattern_len] == '\0' || strchr(word_delim, substr[pattern_len]) != NULL);

        if(starts_on_word_boundary && ends_on_word_boundary)
            return 1;

        substr = strstr(substr+1, pattern);
    }

    return 0;
}

static int
acutest_select_(const char* pattern)
{
    int i;
    int n = 0;

    /* Try exact match. */
    for(i = 0; i < acutest_list_size_; i++) {
        if(strcmp(acutest_list_[i].name, pattern) == 0) {
            acutest_test_data_[i].state = ACUTEST_STATE_SELECTED;
            n++;
            break;
        }
    }
    if(n > 0)
        return n;

    /* Try word match. */
    for(i = 0; i < acutest_list_size_; i++) {
        if(acutest_name_contains_word_(acutest_list_[i].name, pattern)) {
            acutest_test_data_[i].state = ACUTEST_STATE_SELECTED;
            n++;
        }
    }
    if(n > 0)
        return n;

    /* Try relaxed match. */
    for(i = 0; i < acutest_list_size_; i++) {
        if(strstr(acutest_list_[i].name, pattern) != NULL) {
            acutest_test_data_[i].state = ACUTEST_STATE_SELECTED;
            n++;
        }
    }

    return n;
}


/* Called if anything goes bad in Acutest, or if the unit test ends in other
 * way then by normal returning from its function (e.g. exception or some
 * abnormal child process termination). */
static void ACUTEST_ATTRIBUTE_(format (printf, 1, 2))
acutest_error_(const char* fmt, ...)
{
    if(acutest_verbose_level_ == 0)
        return;

    if(acutest_verbose_level_ >= 2) {
        va_list args;

        acutest_line_indent_(1);
        if(acutest_verbose_level_ >= 3)
            acutest_colored_printf_(ACUTEST_COLOR_RED_INTENSIVE_, "ERROR: ");
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    if(acutest_verbose_level_ >= 3) {
        printf("\n");
    }
}

/* Call directly the given test unit function. */
static enum acutest_state_
acutest_do_run_(const struct acutest_test_* test, int index)
{
    enum acutest_state_ state = ACUTEST_STATE_FAILED;

    acutest_current_test_ = test;
    acutest_current_index_ = index;
    acutest_test_failures_ = 0;
    acutest_test_already_logged_ = 0;
    acutest_test_check_count_ = 0;
    acutest_test_skip_count_ = 0;
    acutest_cond_failed_ = 0;

#ifdef __cplusplus
#ifndef TEST_NO_EXCEPTIONS
    try {
#endif
#endif
        acutest_init_(test->name);
        acutest_begin_test_line_(test);

        /* This is good to do in case the test unit crashes. */
        fflush(stdout);
        fflush(stderr);

        if(!acutest_worker_) {
            acutest_abort_has_jmp_buf_ = 1;
            if(setjmp(acutest_abort_jmp_buf_) != 0)
                goto aborted;
        }

        acutest_timer_get_time_(&acutest_timer_start_);
        test->func();

aborted:
        acutest_abort_has_jmp_buf_ = 0;
        acutest_timer_get_time_(&acutest_timer_end_);

        if(acutest_test_failures_ > 0)
            state = ACUTEST_STATE_FAILED;
        else if(acutest_test_skip_count_ > 0)
            state = ACUTEST_STATE_SKIPPED;
        else
            state = ACUTEST_STATE_SUCCESS;

        if(!acutest_test_already_logged_)
            acutest_finish_test_line_(state);

        if(acutest_verbose_level_ >= 3) {
            acutest_line_indent_(1);
            switch(state) {
                case ACUTEST_STATE_SUCCESS:
                    acutest_colored_printf_(ACUTEST_COLOR_GREEN_INTENSIVE_, "SUCCESS: ");
                    printf("All conditions have passed.\n");

                    if(acutest_timer_) {
                        acutest_line_indent_(1);
                        printf("Duration: ");
                        acutest_timer_print_diff_();
                        printf("\n");
                    }
                    break;

                case ACUTEST_STATE_SKIPPED:
                    acutest_colored_printf_(ACUTEST_COLOR_YELLOW_INTENSIVE_, "SKIPPED: ");
                    printf("%s.\n", acutest_test_skip_reason_);
                    break;

                default:
                    acutest_colored_printf_(ACUTEST_COLOR_RED_INTENSIVE_, "FAILED: ");
                    printf("%d condition%s %s failed.\n",
                            acutest_test_failures_,
                            (acutest_test_failures_ == 1) ? "" : "s",
                            (acutest_test_failures_ == 1) ? "has" : "have");
                    break;
            }
            printf("\n");
        }

#ifdef __cplusplus
#ifndef TEST_NO_EXCEPTIONS
    } catch(std::exception& e) {
        const char* what = e.what();
        acutest_check_(0, NULL, 0, "Threw std::exception");
        if(what != NULL)
            acutest_message_("std::exception::what(): %s", what);

        if(acutest_verbose_level_ >= 3) {
            acutest_line_indent_(1);
            acutest_colored_printf_(ACUTEST_COLOR_RED_INTENSIVE_, "FAILED: ");
            printf("C++ exception.\n\n");
        }
    } catch(...) {
        acutest_check_(0, NULL, 0, "Threw an exception");

        if(acutest_verbose_level_ >= 3) {
            acutest_line_indent_(1);
            acutest_colored_printf_(ACUTEST_COLOR_RED_INTENSIVE_, "FAILED: ");
            printf("C++ exception.\n\n");
        }
    }
#endif
#endif

    acutest_fini_(test->name);
    acutest_case_(NULL);
    acutest_current_test_ = NULL;

    return state;
}

/* Trigger the unit test. If possible (and not suppressed) it starts a child
 * process who calls acutest_do_run_(), otherwise it calls acutest_do_run_()
 * directly. */
static void
acutest_run_(const struct acutest_test_* test, int index, int master_index)
{
    enum acutest_state_ state = ACUTEST_STATE_FAILED;
    acutest_timer_type_ start, end;

    acutest_current_test_ = test;
    acutest_test_already_logged_ = 0;
    acutest_timer_get_time_(&start);

    if(!acutest_no_exec_) {

#if defined(ACUTEST_UNIX_)

        pid_t pid;
        int exit_code;

        /* Make sure the child starts with empty I/O buffers. */
        fflush(stdout);
        fflush(stderr);

        pid = fork();
        if(pid == (pid_t)-1) {
            acutest_error_("Cannot fork. %s [%d]", strerror(errno), errno);
        } else if(pid == 0) {
            /* Child: Do the test. */
            acutest_worker_ = 1;
            state = acutest_do_run_(test, index);
            acutest_exit_((int) state);
        } else {
            /* Parent: Wait until child terminates and analyze its exit code. */
            waitpid(pid, &exit_code, 0);
            if(WIFEXITED(exit_code)) {
                state = (enum acutest_state_) WEXITSTATUS(exit_code);
            } else if(WIFSIGNALED(exit_code)) {
                char tmp[32];
                const char* signame;
                switch(WTERMSIG(exit_code)) {
                    case SIGINT:  signame = "SIGINT"; break;
                    case SIGHUP:  signame = "SIGHUP"; break;
                    case SIGQUIT: signame = "SIGQUIT"; break;
                    case SIGABRT: signame = "SIGABRT"; break;
                    case SIGKILL: signame = "SIGKILL"; break;
                    case SIGSEGV: signame = "SIGSEGV"; break;
                    case SIGILL:  signame = "SIGILL"; break;
                    case SIGTERM: signame = "SIGTERM"; break;
                    default:      snprintf(tmp, sizeof(tmp), "signal %d", WTERMSIG(exit_code)); signame = tmp; break;
                }
                acutest_error_("Test interrupted by %s.", signame);
            } else {
                acutest_error_("Test ended in an unexpected way [%d].", exit_code);
            }
        }

#elif defined(ACUTEST_WIN_)

        char buffer[512] = {0};
        STARTUPINFOA startupInfo;
        PROCESS_INFORMATION processInfo;
        DWORD exitCode;

        /* Windows has no fork(). So we propagate all info into the child
         * through a command line arguments. */
        snprintf(buffer, sizeof(buffer),
                 "%s --worker=%d %s --no-exec --no-summary %s --verbose=%d --color=%s -- \"%s\"",
                 acutest_argv0_, index, acutest_timer_ ? "--time" : "",
                 acutest_tap_ ? "--tap" : "", acutest_verbose_level_,
                 acutest_colorize_ ? "always" : "never",
                 test->name);
        memset(&startupInfo, 0, sizeof(startupInfo));
        startupInfo.cb = sizeof(STARTUPINFO);
        if(CreateProcessA(NULL, buffer, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo)) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            GetExitCodeProcess(processInfo.hProcess, &exitCode);
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
            switch(exitCode) {
                case 0:             state = ACUTEST_STATE_SUCCESS; break;
                case 1:             state = ACUTEST_STATE_FAILED; break;
                case 2:             state = ACUTEST_STATE_SKIPPED; break;
                case 3:             acutest_error_("Aborted."); break;
                case 0xC0000005:    acutest_error_("Access violation."); break;
                default:            acutest_error_("Test ended in an unexpected way [%lu].", exitCode); break;
            }
        } else {
            acutest_error_("Cannot create unit test subprocess [%ld].", GetLastError());
        }

#else

        /* A platform where we don't know how to run child process. */
        state = acutest_do_run_(test, index);

#endif

    } else {
        /* Child processes suppressed through --no-exec. */
        state = acutest_do_run_(test, index);
    }
    acutest_timer_get_time_(&end);

    acutest_current_test_ = NULL;

    acutest_test_data_[master_index].state = state;
    acutest_test_data_[master_index].duration = acutest_timer_diff_(start, end);
}

#if defined(ACUTEST_WIN_)
/* Callback for SEH events. */
static LONG CALLBACK
acutest_seh_exception_filter_(EXCEPTION_POINTERS *ptrs)
{
    acutest_check_(0, NULL, 0, "Unhandled SEH exception");
    acutest_message_("Exception code:    0x%08lx", ptrs->ExceptionRecord->ExceptionCode);
    acutest_message_("Exception address: 0x%p", ptrs->ExceptionRecord->ExceptionAddress);

    fflush(stdout);
    fflush(stderr);

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif


#define ACUTEST_CMDLINE_OPTFLAG_OPTIONALARG_    0x0001
#define ACUTEST_CMDLINE_OPTFLAG_REQUIREDARG_    0x0002

#define ACUTEST_CMDLINE_OPTID_NONE_             0
#define ACUTEST_CMDLINE_OPTID_UNKNOWN_          (-0x7fffffff + 0)
#define ACUTEST_CMDLINE_OPTID_MISSINGARG_       (-0x7fffffff + 1)
#define ACUTEST_CMDLINE_OPTID_BOGUSARG_         (-0x7fffffff + 2)

typedef struct acutest_test_CMDLINE_OPTION_ {
    char shortname;
    const char* longname;
    int id;
    unsigned flags;
} ACUTEST_CMDLINE_OPTION_;

static int
acutest_cmdline_handle_short_opt_group_(const ACUTEST_CMDLINE_OPTION_* options,
                    const char* arggroup,
                    int (*callback)(int /*optval*/, const char* /*arg*/))
{
    const ACUTEST_CMDLINE_OPTION_* opt;
    int i;
    int ret = 0;

    for(i = 0; arggroup[i] != '\0'; i++) {
        for(opt = options; opt->id != 0; opt++) {
            if(arggroup[i] == opt->shortname)
                break;
        }

        if(opt->id != 0  &&  !(opt->flags & ACUTEST_CMDLINE_OPTFLAG_REQUIREDARG_)) {
            ret = callback(opt->id, NULL);
        } else {
            /* Unknown option. */
            char badoptname[3];
            badoptname[0] = '-';
            badoptname[1] = arggroup[i];
            badoptname[2] = '\0';
            ret = callback((opt->id != 0 ? ACUTEST_CMDLINE_OPTID_MISSINGARG_ : ACUTEST_CMDLINE_OPTID_UNKNOWN_),
                            badoptname);
        }

        if(ret != 0)
            break;
    }

    return ret;
}

#define ACUTEST_CMDLINE_AUXBUF_SIZE_  32

static int
acutest_cmdline_read_(const ACUTEST_CMDLINE_OPTION_* options, int argc, char** argv,
                      int (*callback)(int /*optval*/, const char* /*arg*/))
{

    const ACUTEST_CMDLINE_OPTION_* opt;
    char auxbuf[ACUTEST_CMDLINE_AUXBUF_SIZE_+1];
    int after_doubledash = 0;
    int i = 1;
    int ret = 0;

    auxbuf[ACUTEST_CMDLINE_AUXBUF_SIZE_] = '\0';

    while(i < argc) {
        if(after_doubledash  ||  strcmp(argv[i], "-") == 0) {
            /* Non-option argument. */
            ret = callback(ACUTEST_CMDLINE_OPTID_NONE_, argv[i]);
        } else if(strcmp(argv[i], "--") == 0) {
            /* End of options. All the remaining members are non-option arguments. */
            after_doubledash = 1;
        } else if(argv[i][0] != '-') {
            /* Non-option argument. */
            ret = callback(ACUTEST_CMDLINE_OPTID_NONE_, argv[i]);
        } else {
            for(opt = options; opt->id != 0; opt++) {
                if(opt->longname != NULL  &&  strncmp(argv[i], "--", 2) == 0) {
                    size_t len = strlen(opt->longname);
                    if(strncmp(argv[i]+2, opt->longname, len) == 0) {
                        /* Regular long option. */
                        if(argv[i][2+len] == '\0') {
                            /* with no argument provided. */
                            if(!(opt->flags & ACUTEST_CMDLINE_OPTFLAG_REQUIREDARG_))
                                ret = callback(opt->id, NULL);
                            else
                                ret = callback(ACUTEST_CMDLINE_OPTID_MISSINGARG_, argv[i]);
                            break;
                        } else if(argv[i][2+len] == '=') {
                            /* with an argument provided. */
                            if(opt->flags & (ACUTEST_CMDLINE_OPTFLAG_OPTIONALARG_ | ACUTEST_CMDLINE_OPTFLAG_REQUIREDARG_)) {
                                ret = callback(opt->id, argv[i]+2+len+1);
                            } else {
                                snprintf(auxbuf, sizeof(auxbuf), "--%s", opt->longname);
                                ret = callback(ACUTEST_CMDLINE_OPTID_BOGUSARG_, auxbuf);
                            }
                            break;
                        } else {
                            continue;
                        }
                    }
                } else if(opt->shortname != '\0'  &&  argv[i][0] == '-') {
                    if(argv[i][1] == opt->shortname) {
                        /* Regular short option. */
                        if(opt->flags & ACUTEST_CMDLINE_OPTFLAG_REQUIREDARG_) {
                            if(argv[i][2] != '\0')
                                ret = callback(opt->id, argv[i]+2);
                            else if(i+1 < argc)
                                ret = callback(opt->id, argv[++i]);
                            else
                                ret = callback(ACUTEST_CMDLINE_OPTID_MISSINGARG_, argv[i]);
                            break;
                        } else {
                            ret = callback(opt->id, NULL);

                            /* There might be more (argument-less) short options
                             * grouped together. */
                            if(ret == 0  &&  argv[i][2] != '\0')
                                ret = acutest_cmdline_handle_short_opt_group_(options, argv[i]+2, callback);
                            break;
                        }
                    }
                }
            }

            if(opt->id == 0) {  /* still not handled? */
                if(argv[i][0] != '-') {
                    /* Non-option argument. */
                    ret = callback(ACUTEST_CMDLINE_OPTID_NONE_, argv[i]);
                } else {
                    /* Unknown option. */
                    char* badoptname = argv[i];

                    if(strncmp(badoptname, "--", 2) == 0) {
                        /* Strip any argument from the long option. */
                        char* assignment = strchr(badoptname, '=');
                        if(assignment != NULL) {
                            size_t len = (size_t)(assignment - badoptname);
                            if(len > ACUTEST_CMDLINE_AUXBUF_SIZE_)
                                len = ACUTEST_CMDLINE_AUXBUF_SIZE_;
                            strncpy(auxbuf, badoptname, len);
                            auxbuf[len] = '\0';
                            badoptname = auxbuf;
                        }
                    }

                    ret = callback(ACUTEST_CMDLINE_OPTID_UNKNOWN_, badoptname);
                }
            }
        }

        if(ret != 0)
            return ret;
        i++;
    }

    return ret;
}

static void
acutest_help_(void)
{
    printf("Usage: %s [options] [test...]\n", acutest_argv0_);
    printf("\n");
    printf("Run the specified unit tests; or if the option '--exclude' is used, run all\n");
    printf("tests in the suite but those listed.  By default, if no tests are specified\n");
    printf("on the command line, all unit tests in the suite are run.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -X, --exclude         Execute all unit tests but the listed ones\n");
    printf("      --exec[=WHEN]     If supported, execute unit tests as child processes\n");
    printf("                          (WHEN is one of 'auto', 'always', 'never')\n");
    printf("  -E, --no-exec         Same as --exec=never\n");
#if defined ACUTEST_WIN_
    printf("  -t, --time            Measure test duration\n");
#elif defined ACUTEST_HAS_POSIX_TIMER_
    printf("  -t, --time            Measure test duration (real time)\n");
    printf("      --time=TIMER      Measure test duration, using given timer\n");
    printf("                          (TIMER is one of 'real', 'cpu')\n");
#endif
    printf("      --no-summary      Suppress printing of test results summary\n");
    printf("      --tap             Produce TAP-compliant output\n");
    printf("                          (See https://testanything.org/)\n");
    printf("  -x, --xml-output=FILE Enable XUnit output to the given file\n");
    printf("  -l, --list            List unit tests in the suite and exit\n");
    printf("  -v, --verbose         Make output more verbose\n");
    printf("      --verbose=LEVEL   Set verbose level to LEVEL:\n");
    printf("                          0 ... Be silent\n");
    printf("                          1 ... Output one line per test (and summary)\n");
    printf("                          2 ... As 1 and failed conditions (this is default)\n");
    printf("                          3 ... As 1 and all conditions (and extended summary)\n");
    printf("  -q, --quiet           Same as --verbose=0\n");
    printf("      --color[=WHEN]    Enable colorized output\n");
    printf("                          (WHEN is one of 'auto', 'always', 'never')\n");
    printf("      --no-color        Same as --color=never\n");
    printf("  -h, --help            Display this help and exit\n");

    if(acutest_list_size_ < 16) {
        printf("\n");
        acutest_list_names_();
    }
}

static const ACUTEST_CMDLINE_OPTION_ acutest_cmdline_options_[] = {
    { 'X',  "exclude",      'X', 0 },
    { 's',  "skip",         'X', 0 },   /* kept for compatibility, use --exclude instead */
    {  0,   "exec",         'e', ACUTEST_CMDLINE_OPTFLAG_OPTIONALARG_ },
    { 'E',  "no-exec",      'E', 0 },
#if defined ACUTEST_WIN_
    { 't',  "time",         't', 0 },
    {  0,   "timer",        't', 0 },   /* kept for compatibility */
#elif defined ACUTEST_HAS_POSIX_TIMER_
    { 't',  "time",         't', ACUTEST_CMDLINE_OPTFLAG_OPTIONALARG_ },
    {  0,   "timer",        't', ACUTEST_CMDLINE_OPTFLAG_OPTIONALARG_ },  /* kept for compatibility */
#endif
    {  0,   "no-summary",   'S', 0 },
    {  0,   "tap",          'T', 0 },
    { 'l',  "list",         'l', 0 },
    { 'v',  "verbose",      'v', ACUTEST_CMDLINE_OPTFLAG_OPTIONALARG_ },
    { 'q',  "quiet",        'q', 0 },
    {  0,   "color",        'c', ACUTEST_CMDLINE_OPTFLAG_OPTIONALARG_ },
    {  0,   "no-color",     'C', 0 },
    { 'h',  "help",         'h', 0 },
    {  0,   "worker",       'w', ACUTEST_CMDLINE_OPTFLAG_REQUIREDARG_ },  /* internal */
    { 'x',  "xml-output",   'x', ACUTEST_CMDLINE_OPTFLAG_REQUIREDARG_ },
    {  0,   NULL,            0,  0 }
};

static int
acutest_cmdline_callback_(int id, const char* arg)
{
    switch(id) {
        case 'X':
            acutest_exclude_mode_ = 1;
            break;

        case 'e':
            if(arg == NULL || strcmp(arg, "always") == 0) {
                acutest_no_exec_ = 0;
            } else if(strcmp(arg, "never") == 0) {
                acutest_no_exec_ = 1;
            } else if(strcmp(arg, "auto") == 0) {
                /*noop*/
            } else {
                fprintf(stderr, "%s: Unrecognized argument '%s' for option --exec.\n", acutest_argv0_, arg);
                fprintf(stderr, "Try '%s --help' for more information.\n", acutest_argv0_);
                acutest_exit_(2);
            }
            break;

        case 'E':
            acutest_no_exec_ = 1;
            break;

        case 't':
#if defined ACUTEST_WIN_  ||  defined ACUTEST_HAS_POSIX_TIMER_
            if(arg == NULL || strcmp(arg, "real") == 0) {
                acutest_timer_ = 1;
    #ifndef ACUTEST_WIN_
            } else if(strcmp(arg, "cpu") == 0) {
                acutest_timer_ = 2;
    #endif
            } else {
                fprintf(stderr, "%s: Unrecognized argument '%s' for option --time.\n", acutest_argv0_, arg);
                fprintf(stderr, "Try '%s --help' for more information.\n", acutest_argv0_);
                acutest_exit_(2);
            }
#endif
            break;

        case 'S':
            acutest_no_summary_ = 1;
            break;

        case 'T':
            acutest_tap_ = 1;
            break;

        case 'l':
            acutest_list_names_();
            acutest_exit_(0);
            break;

        case 'v':
            acutest_verbose_level_ = (arg != NULL ? atoi(arg) : acutest_verbose_level_+1);
            break;

        case 'q':
            acutest_verbose_level_ = 0;
            break;

        case 'c':
            if(arg == NULL || strcmp(arg, "always") == 0) {
                acutest_colorize_ = 1;
            } else if(strcmp(arg, "never") == 0) {
                acutest_colorize_ = 0;
            } else if(strcmp(arg, "auto") == 0) {
                /*noop*/
            } else {
                fprintf(stderr, "%s: Unrecognized argument '%s' for option --color.\n", acutest_argv0_, arg);
                fprintf(stderr, "Try '%s --help' for more information.\n", acutest_argv0_);
                acutest_exit_(2);
            }
            break;

        case 'C':
            acutest_colorize_ = 0;
            break;

        case 'h':
            acutest_help_();
            acutest_exit_(0);
            break;

        case 'w':
            acutest_worker_ = 1;
            acutest_worker_index_ = atoi(arg);
            break;
        case 'x':
            acutest_xml_output_ = fopen(arg, "w");
            if (!acutest_xml_output_) {
                fprintf(stderr, "Unable to open '%s': %s\n", arg, strerror(errno));
                acutest_exit_(2);
            }
            break;

        case 0:
            if(acutest_select_(arg) == 0) {
                fprintf(stderr, "%s: Unrecognized unit test '%s'\n", acutest_argv0_, arg);
                fprintf(stderr, "Try '%s --list' for list of unit tests.\n", acutest_argv0_);
                acutest_exit_(2);
            }
            break;

        case ACUTEST_CMDLINE_OPTID_UNKNOWN_:
            fprintf(stderr, "Unrecognized command line option '%s'.\n", arg);
            fprintf(stderr, "Try '%s --help' for more information.\n", acutest_argv0_);
            acutest_exit_(2);
            break;

        case ACUTEST_CMDLINE_OPTID_MISSINGARG_:
            fprintf(stderr, "The command line option '%s' requires an argument.\n", arg);
            fprintf(stderr, "Try '%s --help' for more information.\n", acutest_argv0_);
            acutest_exit_(2);
            break;

        case ACUTEST_CMDLINE_OPTID_BOGUSARG_:
            fprintf(stderr, "The command line option '%s' does not expect an argument.\n", arg);
            fprintf(stderr, "Try '%s --help' for more information.\n", acutest_argv0_);
            acutest_exit_(2);
            break;
    }

    return 0;
}

static int
acutest_under_debugger_(void)
{
#ifdef ACUTEST_LINUX_
    /* Scan /proc/self/status for line "TracerPid: [PID]". If such line exists
     * and the PID is non-zero, we're being debugged. */
    {
        static const int OVERLAP = 32;
        int fd;
        char buf[512];
        size_t n_read;
        pid_t tracer_pid = 0;

        /* Little trick so that we can treat the 1st line the same as any other
         * and detect line start easily. */
        buf[0] = '\n';
        n_read = 1;

        fd = open("/proc/self/status", O_RDONLY);
        if(fd != -1) {
            while(1) {
                static const char pattern[] = "\nTracerPid:";
                const char* field;

                while(n_read < sizeof(buf) - 1) {
                    ssize_t n;

                    n = read(fd, buf + n_read, sizeof(buf) - 1 - n_read);
                    if(n <= 0)
                        break;
                    n_read += (size_t)n;
                }
                buf[n_read] = '\0';

                field = strstr(buf, pattern);
                if(field != NULL  &&  field < buf + sizeof(buf) - OVERLAP) {
                    tracer_pid = (pid_t) atoi(field + sizeof(pattern) - 1);
                    break;
                }

                if(n_read == sizeof(buf) - 1) {
                    /* Move the tail with the potentially incomplete line we're
                     * be looking for to the beginning of the buffer.
                     * (The OVERLAP must be large enough so the searched line
                     * can fit in completely.) */
                    memmove(buf, buf + sizeof(buf) - 1 - OVERLAP, OVERLAP);
                    n_read = OVERLAP;
                } else {
                    break;
                }
            }

            close(fd);

            if(tracer_pid != 0)
                return 1;
        }
    }
#endif

#ifdef ACUTEST_MACOS_
    /* See https://developer.apple.com/library/archive/qa/qa1361/_index.html */
    {
        int mib[4];
        struct kinfo_proc info;
        size_t size;

        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PID;
        mib[3] = getpid();

        size = sizeof(info);
        info.kp_proc.p_flag = 0;
        sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);

        if(info.kp_proc.p_flag & P_TRACED)
            return 1;
    }
#endif

#ifdef ACUTEST_WIN_
    if(IsDebuggerPresent())
        return 1;
#endif

#ifdef RUNNING_ON_VALGRIND
    /* We treat Valgrind as a debugger of sorts.
     * (Macro RUNNING_ON_VALGRIND is provided by <valgrind.h>, if available.) */
    if(RUNNING_ON_VALGRIND)
        return 1;
#endif

    return 0;
}

int
main(int argc, char** argv)
{
    int i, index;
    int exit_code = 1;

    acutest_argv0_ = argv[0];

#if defined ACUTEST_UNIX_
    acutest_colorize_ = isatty(STDOUT_FILENO);
#elif defined ACUTEST_WIN_
 #if defined _BORLANDC_
    acutest_colorize_ = isatty(_fileno(stdout));
 #else
    acutest_colorize_ = _isatty(_fileno(stdout));
 #endif
#else
    acutest_colorize_ = 0;
#endif

    /* Count all test units */
    acutest_list_size_ = 0;
    for(i = 0; acutest_list_[i].func != NULL; i++)
        acutest_list_size_++;

    acutest_test_data_ = (struct acutest_test_data_*)calloc(acutest_list_size_, sizeof(struct acutest_test_data_));
    if(acutest_test_data_ == NULL) {
        fprintf(stderr, "Out of memory.\n");
        acutest_exit_(2);
    }

    /* Parse options */
    acutest_cmdline_read_(acutest_cmdline_options_, argc, argv, acutest_cmdline_callback_);

    /* Initialize the proper timer. */
    acutest_timer_init_();

#if defined(ACUTEST_WIN_)
    SetUnhandledExceptionFilter(acutest_seh_exception_filter_);
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
#endif

    /* Determine what to run. */
    if(acutest_count_(ACUTEST_STATE_SELECTED) > 0) {
        enum acutest_state_ if_selected;
        enum acutest_state_ if_unselected;

        if(!acutest_exclude_mode_) {
            if_selected = ACUTEST_STATE_NEEDTORUN;
            if_unselected = ACUTEST_STATE_EXCLUDED;
        } else {
            if_selected = ACUTEST_STATE_EXCLUDED;
            if_unselected = ACUTEST_STATE_NEEDTORUN;
        }

        for(i = 0; acutest_list_[i].func != NULL; i++) {
            if(acutest_test_data_[i].state == ACUTEST_STATE_SELECTED)
                acutest_test_data_[i].state = if_selected;
            else
                acutest_test_data_[i].state = if_unselected;
        }
    } else {
        /* By default, we want to run all tests. */
        for(i = 0; acutest_list_[i].func != NULL; i++)
            acutest_test_data_[i].state = ACUTEST_STATE_NEEDTORUN;
    }

    /* By default, we want to suppress running tests as child processes if we
     * run just one test, or if we're under debugger: Debugging tests is then
     * so much easier. */
    if(acutest_no_exec_ < 0) {
        if(acutest_count_(ACUTEST_STATE_NEEDTORUN) <= 1  ||  acutest_under_debugger_())
            acutest_no_exec_ = 1;
        else
            acutest_no_exec_ = 0;
    }

    if(acutest_tap_) {
        /* TAP requires we know test result ("ok", "not ok") before we output
         * anything about the test, and this gets problematic for larger verbose
         * levels. */
        if(acutest_verbose_level_ > 2)
            acutest_verbose_level_ = 2;

        /* TAP harness should provide some summary. */
        acutest_no_summary_ = 1;

        if(!acutest_worker_)
            printf("1..%d\n", acutest_count_(ACUTEST_STATE_NEEDTORUN));
    }

    index = acutest_worker_index_;
    for(i = 0; acutest_list_[i].func != NULL; i++) {
        if(acutest_test_data_[i].state == ACUTEST_STATE_NEEDTORUN)
            acutest_run_(&acutest_list_[i], index++, i);
    }

    /* Write a summary */
    if(!acutest_no_summary_ && acutest_verbose_level_ >= 1) {
        int n_run, n_success, n_failed ;

        n_run = acutest_list_size_ - acutest_count_(ACUTEST_STATE_EXCLUDED);
        n_success = acutest_count_(ACUTEST_STATE_SUCCESS);
        n_failed = acutest_count_(ACUTEST_STATE_FAILED);

        if(acutest_verbose_level_ >= 3) {
            acutest_colored_printf_(ACUTEST_COLOR_DEFAULT_INTENSIVE_, "Summary:\n");

            printf("  Count of run unit tests:        %4d\n", n_run);
            printf("  Count of successful unit tests: %4d\n", n_success);
            printf("  Count of failed unit tests:     %4d\n", n_failed);
        }

        if(n_failed == 0) {
            acutest_colored_printf_(ACUTEST_COLOR_GREEN_INTENSIVE_, "SUCCESS:");
            printf(" No unit tests have failed.\n");
        } else {
            acutest_colored_printf_(ACUTEST_COLOR_RED_INTENSIVE_, "FAILED:");
            printf(" %d of %d unit tests %s failed.\n",
                    n_failed, n_run, (n_failed == 1) ? "has" : "have");
        }

        if(acutest_verbose_level_ >= 3)
            printf("\n");
    }

    if (acutest_xml_output_) {
        const char* suite_name = acutest_basename_(argv[0]);
        fprintf(acutest_xml_output_, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(acutest_xml_output_, "<testsuite name=\"%s\" tests=\"%d\" errors=\"0\" failures=\"%d\" skip=\"%d\">\n",
            suite_name,
            (int)acutest_list_size_,
            acutest_count_(ACUTEST_STATE_FAILED),
            acutest_count_(ACUTEST_STATE_SKIPPED) + acutest_count_(ACUTEST_STATE_EXCLUDED));
        for(i = 0; acutest_list_[i].func != NULL; i++) {
            struct acutest_test_data_ *details = &acutest_test_data_[i];
            const char* str_state;
            fprintf(acutest_xml_output_, "  <testcase name=\"%s\" time=\"%.2f\">\n", acutest_list_[i].name, details->duration);

            switch(details->state) {
                case ACUTEST_STATE_SUCCESS:     str_state = NULL; break;
                case ACUTEST_STATE_EXCLUDED:    /* Fall through. */
                case ACUTEST_STATE_SKIPPED:     str_state = "<skipped />"; break;
                case ACUTEST_STATE_FAILED:      /* Fall through. */
                default:                        str_state = "<failure />"; break;
            }

            if(str_state != NULL)
                fprintf(acutest_xml_output_, "    %s\n", str_state);
            fprintf(acutest_xml_output_, "  </testcase>\n");
        }
        fprintf(acutest_xml_output_, "</testsuite>\n");
        fclose(acutest_xml_output_);
    }

    if(acutest_worker_  &&  acutest_count_(ACUTEST_STATE_EXCLUDED)+1 == acutest_list_size_) {
        /* If we are the child process, we need to propagate the test state
         * without any moderation. */
        for(i = 0; acutest_list_[i].func != NULL; i++) {
            if(acutest_test_data_[i].state != ACUTEST_STATE_EXCLUDED) {
                exit_code = (int) acutest_test_data_[i].state;
                break;
            }
        }
    } else {
        if(acutest_count_(ACUTEST_STATE_FAILED) > 0)
            exit_code = 1;
        else
            exit_code = 0;
    }

    acutest_cleanup_();
    return exit_code;
}


#endif  /* #ifndef TEST_NO_MAIN */

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#ifdef __cplusplus
    }  /* extern "C" */
#endif

#endif  /* #ifndef ACUTEST_H */
