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

#include <functional>
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

} //namespace utils


#endif //#define INCLUDED_utils_hpp_35e6662d_e797_47c7_8019_78c3725e0be3
