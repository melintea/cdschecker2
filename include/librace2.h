/*
 *  $Id: $
 *
 *  Copyright 2024 Aurelian Melinte.
 *  Released under GPL 3.0 or later.
 *
 *  You need a C++0x compiler.
 *
 */

#ifndef INCLUDED_librace2_hpp_e8be4557_fdea_42b4_8df5_a1660e69d955
#define INCLUDED_librace2_hpp_e8be4557_fdea_42b4_8df5_a1660e69d955

#pragma once

#include "common.h"
#include "librace.h"
#include "model.h"

#include <cstdint>
#include <functional>
#include <type_traits>

//#include <iostream>

#pragma GCC push_options
#pragma GCC optimize("O0") // do not discard any of the checks

namespace librace {

/*
 * Helpers
 */

template <typename T>
T load(const void* addr, typename std::enable_if<sizeof(T) == 1, bool>::type = true) {
    return (T)load_8(addr);
}

template <typename T>
T load(const void* addr, typename std::enable_if<sizeof(T) == 2, bool>::type = true) {
    return (T)load_16(addr);
}

template <typename T>
T load(const void* addr, typename std::enable_if<sizeof(T) == 4, bool>::type = true) {
    return (T)load_32(addr);
}

template <typename T >
T load(const void* addr, typename std::enable_if<sizeof(T) == 8, bool>::type = true) {
    return (T)load_64(addr);
}

template <typename T>
void store(T* addr, T val, typename std::enable_if<(sizeof(T) == 1), bool>::type = true) {
    store_8((void*)addr, (uint8_t)val);
}

template <typename T>
void store(T* addr, T val, typename std::enable_if<(sizeof(T) == 2), bool>::type = true) {
    store_16((void*)addr, (uint16_t)val);
}

template <typename T>
void store(T* addr, T val, typename std::enable_if_t<(sizeof(T) == 4), bool> = true) {
    store_32((void*)addr, (uint32_t)val);
}

template <typename T>
void store(T* addr, T val, typename std::enable_if<(sizeof(T) == 8), bool>::type = true) {
    return (T)store_64((void*)addr, (uint64_t)val);
}


/*
 * Check fundamental types for data races.
 * Usage:
 *   librace::var<int> x  = 0;   // int  x = 0;
 *   librace::ref<int> rx = x;   // int& rx = x
 *   librace::pre<int> px = &x;  // int* px = &x;
 */
template<typename T> class var;
template<typename T> class ref;
template<typename T> class ptr;
 
template<typename T>
class var {

    T _value;

    friend class ref<T>;
    friend class ptr<T>;
    
    T load() const {
        static_assert(std::is_integral<T>::value);
        constexpr auto sz(sizeof(T));
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");
        return librace::load<T>(&_value);
    }

    void store(T val) {
        static_assert(std::is_integral<T>::value);
        constexpr auto sz(sizeof(T));
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");
        return librace::store<T>(&_value, val);
    }

public:

    var(T v) :_value(v) {}
    var()                      = delete;
    ~var()                     = default;
    var(const var&)            = delete;
    var& operator=(const var&) = delete;
    var(var&&)                 = default;
    var& operator=(var&&)      = default;

    explicit constexpr operator T() const {return load();}

    friend bool operator==(const var& lhs, const var& rhs)  {return lhs.load() == rhs.load();}
    friend bool operator==(const var& lhs, const T&   rhs)  {return lhs.load() == rhs;}
    friend bool operator==(const T&   lhs, const var& rhs)  {return lhs == rhs.load();}

    //modifiers
    var& operator=(T v)   {store(v); return *this;}
    var& operator+=(T v)  {store((load()+v)); return *this;}
    var& operator-=(T v)  {store((load()-v)); return *this;}
    var& operator*=(T v)  {store((load()*v)); return *this;}
    var& operator/=(T v)  {store((load()/v)); return *this;}
    var& operator%=(T v)  {store((load()%v)); return *this;}
    var& operator++()     {store((load()+1)); return *this;} // ++i
    var& operator--()     {store((load()-1)); return *this;}
    T    operator++(int)  {auto ov(load()); store((ov+1)); return ov;} //i++
    T    operator--(int)  {auto ov(load()); store((ov-1)); return ov;}
    var& operator&=(T v)  {store((load()&v)); return *this;}
    var& operator|=(T v)  {store((load()|v)); return *this;}
    var& operator^=(T v)  {store((load()^v)); return *this;}
    var& operator<<=(T v) {store((load()<<v)); return *this;}
    var& operator>>=(T v) {store((load()>>v)); return *this;}

    //accessors
    T operator+() const {return +load();}
    T operator-() const {return -load();}
    T operator!() const {return !load();}
    T operator~() const {return ~load();}

    //friends
    friend T operator+(var iw, var v)  {return iw.load()+v.load();}
    friend T operator+(var iw, T v)    {return iw.load()+v;}
    friend T operator+(T v, var iw)    {return v+iw.load();}
    friend T operator-(var iw, var v)  {return iw.load()-v.load();}
    friend T operator-(var iw, T v)    {return iw.load()-v;}
    friend T operator-(T v, var iw)    {return v-iw.load();}
    friend T operator*(var iw, var v)  {return iw.load()*v.load();}
    friend T operator*(var iw, T v)    {return iw.load()*v;}
    friend T operator*(T v, var iw)    {return v*iw.load();}
    friend T operator/(var iw, var v)  {return iw.load()/v.load();}
    friend T operator/(var iw, T v)    {return iw.load()/v;}
    friend T operator/(T v, var iw)    {return v/iw.load();}
    friend T operator%(var iw, var v)  {return iw.load()%v.load();}
    friend T operator%(var iw, T v)    {return iw.load()%v;}
    friend T operator%(T v, var iw)    {return v%iw.load();}
    friend T operator&(var iw, var v)  {return iw.load()&v.load();}
    friend T operator&(var iw, T v)    {return iw.load()&v;}
    friend T operator&(T v, var iw)    {return v&iw.load();}
    friend T operator|(var iw, var v)  {return iw.load()|v.load();}
    friend T operator|(var iw, T v)    {return iw.load()|v;}
    friend T operator|(T v, var iw)    {return v|iw.load();}
    friend T operator^(var iw, var v)  {return iw.load()^v.load();}
    friend T operator^(var iw, T v)    {return iw.load()^v;}
    friend T operator^(T v, var iw)    {return v^iw.load();}
    friend T operator<<(var iw, var v) {return iw.load()<<v.load();}
    friend T operator<<(var iw, T v)   {return iw.load()<<v;}
    friend T operator<<(T v, var iw)   {return v<<iw.load();}
    friend T operator>>(var iw, var v) {return iw.load()>>v.load();}
    friend T operator>>(var iw, T v)   {return iw.load()>>v;}
    friend T operator>>(T v, var iw)   {return v>>iw.load();}

}; // var


template<typename T>
class ref {

    var<T>& _value;

    T load() const {
        static_assert(std::is_integral<T>::value);
        constexpr auto sz(sizeof(T));
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");
        return _value.load();
    }

    void store(T val) {
        static_assert(std::is_integral<T>::value);
        constexpr auto sz(sizeof(T));
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");
        return _value.store(val);
    }

public:

    ref(var<T>& v) :_value(v) {
        if (model) { // assert at threads.cc line 37 
            [[maybe_unused]] auto ignored(load()); // mimic ref behavior
	    }
    }
    
    ref()                      = delete;
    ~ref()                     = default;
    ref(const ref&)            = default; // TODO: this implies a read+store
    ref& operator=(const ref&) = default;
    ref(ref&&)                 = default;
    ref& operator=(ref&&)      = default;

    explicit constexpr operator T() const {return load();}

    friend bool operator==(const ref& lhs, const ref& rhs)  {return lhs.load() == rhs.load();}
    friend bool operator==(const ref& lhs, const T&   rhs)  {return lhs.load() == rhs;}
    friend bool operator==(const T&   lhs, const ref& rhs)  {return lhs == rhs.load();}

    //modifiers
    ref& operator=(T v)   {store(v); return *this;}
    ref& operator+=(T v)  {store((load()+v)); return *this;}
    ref& operator-=(T v)  {store((load()-v)); return *this;}
    ref& operator*=(T v)  {store((load()*v)); return *this;}
    ref& operator/=(T v)  {store((load()/v)); return *this;}
    ref& operator%=(T v)  {store((load()%v)); return *this;}
    ref& operator++()     {store((load()+1)); return *this;} // ++i
    ref& operator--()     {store((load()-1)); return *this;}
    T    operator++(int)  {auto ov(load()); store((ov+1)); return ov;} //i++
    T    operator--(int)  {auto ov(load()); store((ov-1)); return ov;}
    ref& operator&=(T v)  {store((load()&v)); return *this;}
    ref& operator|=(T v)  {store((load()|v)); return *this;}
    ref& operator^=(T v)  {store((load()^v)); return *this;}
    ref& operator<<=(T v) {store((load()<<v)); return *this;}
    ref& operator>>=(T v) {store((load()>>v)); return *this;}

    //accessors
    T operator+() const {return +load();}
    T operator-() const {return -load();}
    T operator!() const {return !load();}
    T operator~() const {return ~load();}

    //friends
    friend T operator+(ref iw, ref v)  {return iw.load()+v.load();}
    friend T operator+(ref iw, T v)    {return iw.load()+v;}
    friend T operator+(T v, ref iw)    {return v+iw.load();}
    friend T operator-(ref iw, ref v)  {return iw.load()-v.load();}
    friend T operator-(ref iw, T v)    {return iw.load()-v;}
    friend T operator-(T v, ref iw)    {return v-iw.load();}
    friend T operator*(ref iw, ref v)  {return iw.load()*v.load();}
    friend T operator*(ref iw, T v)    {return iw.load()*v;}
    friend T operator*(T v, ref iw)    {return v*iw.load();}
    friend T operator/(ref iw, ref v)  {return iw.load()/v.load();}
    friend T operator/(ref iw, T v)    {return iw.load()/v;}
    friend T operator/(T v, ref iw)    {return v/iw.load();}
    friend T operator%(ref iw, ref v)  {return iw.load()%v.load();}
    friend T operator%(ref iw, T v)    {return iw.load()%v;}
    friend T operator%(T v, ref iw)    {return v%iw.load();}
    friend T operator&(ref iw, ref v)  {return iw.load()&v.load();}
    friend T operator&(ref iw, T v)    {return iw.load()&v;}
    friend T operator&(T v, ref iw)    {return v&iw.load();}
    friend T operator|(ref iw, ref v)  {return iw.load()|v.load();}
    friend T operator|(ref iw, T v)    {return iw.load()|v;}
    friend T operator|(T v, ref iw)    {return v|iw.load();}
    friend T operator^(ref iw, ref v)  {return iw.load()^v.load();}
    friend T operator^(ref iw, T v)    {return iw.load()^v;}
    friend T operator^(T v, ref iw)    {return v^iw.load();}
    friend T operator<<(ref iw, ref v) {return iw.load()<<v.load();}
    friend T operator<<(ref iw, T v)   {return iw.load()<<v;}
    friend T operator<<(T v, ref iw)   {return v<<iw.load();}
    friend T operator>>(ref iw, ref v) {return iw.load()>>v.load();}
    friend T operator>>(ref iw, T v)   {return iw.load()>>v;}
    friend T operator>>(T v, ref iw)   {return v>>iw.load();}

}; // var


template <typename T>
struct arrow_proxy
{
    ref<T> r;
    
    ref<T>* operator->() {
        return &r;
    }
    
}; // arrow_proxy


/*
 * Check for access both the pointer and the accessed location.
 */
template <class T>
class ptr
{
private:

    var<T>* _ptr = nullptr;

    T* load_as_ptr() const {
        static_assert(std::is_integral<T>::value);
        constexpr auto sz(sizeof(void*));
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T* must be 8/16/32/64 bits");
        return reinterpret_cast<T*>(librace::load<T>(&_ptr)); 
    }

    void store_as_ptr(T* val) {
        static_assert(std::is_integral<T>::value);
        constexpr auto sz(sizeof(void*));
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T* must be 8/16/32/64 bits");
        return librace::store<T>(&_ptr, val); 
    }

    T load_as_val() const {
        constexpr auto sz(sizeof(T));
        static_assert(std::is_integral<T>::value);
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");

        T* p(load_as_ptr());
        return librace::load<T>(p);
    }

    void store_as_val(T val) {
        constexpr auto sz(sizeof(T));
        static_assert(std::is_integral<T>::value);
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");

        T* p(load_as_ptr());
        return librace::store<T>(p, val);
    }

public:

    ptr(var<T>* p) : _ptr(p) {
        if (model) { // assert at threads.cc line 37 
            [[maybe_unused]] auto ignored(load_as_ptr()); 
	    }
    }

    ptr()                      = delete;
    ~ptr()                     = default;

    ptr(const ptr& other) { store_as_ptr(other.load_as_ptr()); }

    ptr& operator=(const ptr& other) { store_as_ptr(other.load_as_ptr());  return *this;}

    ptr(ptr&& other) { store_as_ptr(other.load_as_ptr()); } 

    void operator=(ptr&& other) { store_as_ptr(other.load_as_ptr()); }

    void operator=(T* val) {
        store_as_ptr(val);
    }

    // Technically not needed for integral types
    arrow_proxy<T> operator->() {
        load_as_val(); // To check acces to the var as well
        return load_as_ptr();
    }

    // TODO: *p = val; would bypass the val store check; to fix: *p should return a librace::ref(val).
    ref<T> operator*() {
        return ref(*_ptr); //load_as_val();
    }

}; //ptr


/*
 TODO

template <class T>
class my_unique_ptr<T[]>
{
private:
    T* _ptr = nullptr;

public:

    T& operator[](int index)
    {
        if(index < 0)
        {
            // Throw negative index exception
            throw(new std::exception("Negative index exception"));
        }
        return this->_ptr[index]; // doesn't check out of bounds
    }

    ~my_unique_ptr() // destructor
    {
        __cleanup__();
    }

private:

}; //ptr[] 
*/

} //namespace librace

#pragma GCC pop_options


#endif //#define INCLUDED_librace2_hpp_e8be4557_fdea_42b4_8df5_a1660e69d955
