/*
 *  $Id: $
 *
 *  Copyright 2024 Aurelian Melinte.
 *  Released under GPL 3.0 or later.
 *
 *  You need a C++0x compiler.
 *
 */

#ifndef INCLUDED_utils_hpp_35e6662d_e797_47c7_8019_78c3725e0be3
#define INCLUDED_utils_hpp_35e6662d_e797_47c7_8019_78c3725e0be3

#pragma once

#include "common.h"

#include <functional>
#include <string>
#include <utility>

namespace utils {

/*
 * Simple deleter.
 */
template <typename T>
class ptr_guard
{
    T* _ptr{nullptr};

public:

    ptr_guard(T* ptr = nullptr) : _ptr(ptr) {}
    ~ptr_guard() { delete _ptr; }

    ptr_guard( const ptr_guard& other )            = delete;
    ptr_guard& operator=( const ptr_guard& other ) = delete;

    ptr_guard( ptr_guard&& other ) { std::swap(_ptr, other._ptr); }
    ptr_guard& operator=( ptr_guard&& other ) 
    {
        if (this != &other) {
            std::swap(_ptr, other._ptr);
        }
        return *this;
    }

    const T* ptr() const { return _ptr; }
    T* ptr() { return _ptr; }
}; // ptr_guard

struct scope_print
{
    char _buf[128]{0};

    template <typename... ARGS>
    scope_print(const char* fmt, ARGS... args) {
        ::snprintf(_buf, 128, fmt, args...);
        model_print("-[ %s" , _buf);
    }
    scope_print(const char* str) {
        ::snprintf(_buf, 128, "%s", str);
        model_print("-[ %s" , _buf);
    }
    ~scope_print() {
        model_print("-] %s" , _buf);
    }

    scope_print( const scope_print& other )            = delete;
    scope_print& operator=( const scope_print& other ) = delete;

    scope_print( scope_print&& other )                 = delete;
    scope_print& operator=( scope_print&& other )      = delete;
};

struct scope_debug
{
    char _buf[128]{0};

    template <typename... ARGS>
    scope_debug(const char* fmt, ARGS... args) {
        ::snprintf(_buf, 128, fmt, args...);
        DEBUG("-[ %s" , _buf);
    }
    scope_debug(const char* str) {
        ::snprintf(_buf, 128, "%s", str);
        DEBUG("-[ %s" , _buf);
    }
    ~scope_debug() {
        DEBUG("-] %s" , _buf);
    }

    scope_debug( const scope_debug& other )            = delete;
    scope_debug& operator=( const scope_debug& other ) = delete;

    scope_debug( scope_debug&& other )                 = delete;
    scope_debug& operator=( scope_debug&& other )      = delete;
};

} //namespace utils


#endif //#define INCLUDED_utils_hpp_35e6662d_e797_47c7_8019_78c3725e0be3
