/*
 *  $Id: $
 *
 *  Copyright 2024 Aurelian Melinte.
 *  Released under GPL 3.0 or later.
 *
 *  You need a C++0x compiler.
 *
 */

#ifndef INCLUDED_callstack_hpp_27adaa74_b379_416c_87db_ddef6467210d
#define INCLUDED_callstack_hpp_27adaa74_b379_416c_87db_ddef6467210d

#pragma once

#include "common.h"
#include "model-assert.h"

#include <cstring>
#include <sstream>

#include <cxxabi.h>
#include <stdio.h>
#include <unistd.h>

#include /*lib*/<backtrace.h>
#include /*lib*/<backtrace-supported.h>

namespace cdschecker {


// For a good stack avoid local static funcs & skip frames
class callstack
{
public:

    callstack()
    {
        collect();
    }

    ~callstack()                           = default; // leaks _btstate?
    callstack(const callstack&)            = default;
    callstack& operator=(const callstack&) = default;
    callstack(callstack&&)                 = default;
    callstack& operator=(callstack&&)      = default;

    void print(int fd) const
    {
        switch_alloc = 1; 
        dprintf(fd, "%s", _buf);
        switch_alloc = 0;
    }

private:

    void collect()
    {
        _btstate = backtrace_create_state(nullptr, BACKTRACE_SUPPORTS_THREADS, error_callback, nullptr);
        
        // For a good stack avoid local static funcs & skip frames
        backtrace_full(_btstate, 0, full_callback, error_callback, this); 
        if (_bufUsed < _BUFSZ) {
            _bufUsed += snprintf(_buf+_bufUsed, _BUFSZ-_bufUsed, "\n");
        }
    }

    static void error_callback (void *data, const char *message, int errnum) 
    {
        callstack* cstk(reinterpret_cast<callstack*>(data));
        MODEL_ASSERT(cstk);

        int n{0};
        if (errnum == -1) {
            n = snprintf(cstk->_buf+cstk->_bufUsed, cstk->_BUFSZ-cstk->_bufUsed, 
                         "If you want backtraces, you have to compile with -g\n");
        } else {
            n = snprintf(cstk->_buf+cstk->_bufUsed, cstk->_BUFSZ-cstk->_bufUsed,
                        "Backtrace error %d: %s\n", errnum, (message ? message : ""));
        };
        if (n >= 0) {
            cstk->_bufUsed += n;
            MODEL_ASSERT(cstk->_bufUsed <= cstk->_BUFSZ);
        }
    }

    static void syminfo_callback (void *data, uintptr_t pc, const char *symname, uintptr_t symval, uintptr_t symsize) 
    {
        callstack* cstk(reinterpret_cast<callstack*>(data));
        MODEL_ASSERT(cstk);

        int n{0};
        n = snprintf(cstk->_buf+cstk->_bufUsed, cstk->_BUFSZ-cstk->_bufUsed, 
                        "%lx %s ??:0\n", (unsigned long)pc, symname ? symname : "???");

        if (n >= 0) {
            cstk->_bufUsed += n;
            MODEL_ASSERT(cstk->_bufUsed <= cstk->_BUFSZ);
        }
    }

    static int full_callback (void *data, uintptr_t pc, const char *pathname, int linenum, const char *function) 
    {
        callstack* cstk(reinterpret_cast<callstack*>(data));
        MODEL_ASSERT(cstk);

#if 0
        if ( ! function) {
            backtrace_syminfo(cstk->_btstate, pc, syminfo_callback, error_callback, data);
            return 0;
        }
#endif

        const char *filename = rindex(pathname ? pathname : "", '/');
        if (filename) {
            ++filename; 
        } else {
            filename = pathname ? pathname : "???";
        }
        
        int   rval{0};
        char* dfunc(abi::__cxa_demangle(function ? function : "", nullptr, nullptr, &rval));
        const char *fname = rval == 0 ? dfunc : function;
        
        int n{0};
        n = snprintf(cstk->_buf+cstk->_bufUsed, cstk->_BUFSZ-cstk->_bufUsed,
                     "%s:%d  %s\n", filename, linenum, fname);

      if (n >= 0) {
          cstk->_bufUsed += n;
          MODEL_ASSERT(cstk->_bufUsed <= cstk->_BUFSZ);
      }
    
      return 0;
    }

private:

    struct backtrace_state* _btstate{nullptr}; //TODO: leaks?
    
    static constexpr int _BUFSZ = 4096;
    char                 _buf[_BUFSZ+1] = {0};
    int                  _bufUsed{0};

}; // callstack


} //namespace cdschecker


#endif //#define INCLUDED_callstack_hpp_27adaa74_b379_416c_87db_ddef6467210d
