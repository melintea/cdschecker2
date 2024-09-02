// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0

#ifndef __STACKTRACE_H__
#define __STACKTRACE_H__

#include "callstack.h"

/**
 * @brief Print a demangled stack backtrace of the caller function to file
 * descriptor fd.
 */
static inline void print_stacktrace(int fd = STDERR_FILENO, unsigned int max_frames = 63)
{
	dprintf(fd, "stack trace:\n");
	cdschecker::callstack().print(fd);
}

static inline void print_stacktrace(FILE *out, unsigned int max_frames = 63)
{
	print_stacktrace(fileno(out), max_frames);
}

#endif // __STACKTRACE_H__
