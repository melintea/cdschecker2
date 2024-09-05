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

#include <functional>
#include <type_traits>

//#include <iostream>
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
 */
template<typename T>
class var {

    T _value;

public:

    var() :_value() {}
    var(T v) :_value(v) {}
    operator T() const {return _value;}


    T load() {
        constexpr auto sz(sizeof(T));
        static_assert(std::is_integral<T>::value);
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");
        return librace::load<T>(&_value);
    }

    void store(T val) {
        constexpr auto sz(sizeof(T));
        static_assert(std::is_integral<T>::value);
        static_assert(sz==1 || sz==2 || sz==4 || sz==8, "T must be 8/16/32/64 bits");
        return librace::store<T>(&_value, val);
    }

    //modifiers
    var& operator=(T v)   {store(v); return *this;}
    var& operator+=(T v)  {store((load()+v)); return *this;}
    var& operator-=(T v)  {store((load()-v)); return *this;}
    var& operator*=(T v)  {store((load()*v)); return *this;}
    var& operator/=(T v)  {store((load()/v)); return *this;}
    var& operator%=(T v)  {store((load()%v)); return *this;}
    var& operator++()     {store((load()+1)); return *this;} // ++i
    var& operator--()     {store((load()-1)); return *this;}
    var operator++(int)   {auto ov(load()); store((ov+1)); return ov;} //i++
    var operator--(int)   {auto ov(load()); store((ov-1)); return ov;}
    var& operator&=(T v)  {store((load()&v)); return *this;}
    var& operator|=(T v)  {store((load()|v)); return *this;}
    var& operator^=(T v)  {store((load()^v)); return *this;}
    var& operator<<=(T v) {store((load()<<v)); return *this;}
    var& operator>>=(T v) {store((load()>>v)); return *this;}

    //accessors
    var operator+() const {return var(+load());}
    var operator-() const {return var(-load());}
    var operator!() const {return var(!load());}
    var operator~() const {return var(~load());}

    //friends
    friend var operator+(var iw, var v)  {return var(iw.load()+v.load());}
    friend var operator+(var iw, T v)    {return var(iw.load()+v);}
    friend var operator+(T v, var iw)    {return var(v+iw.load());}
    friend var operator-(var iw, var v)  {return var(iw.load()-v.load());}
    friend var operator-(var iw, T v)    {return var(iw.load()-v);}
    friend var operator-(T v, var iw)    {return var(v-iw.load());}
    friend var operator*(var iw, var v)  {return var(iw.load()*v.load());}
    friend var operator*(var iw, T v)    {return var(iw.load()*v);}
    friend var operator*(T v, var iw)    {return var(v*iw.load());}
    friend var operator/(var iw, var v)  {return var(iw.load()/v.load());}
    friend var operator/(var iw, T v)    {return var(iw.load()/v);}
    friend var operator/(T v, var iw)    {return var(v/iw.load());}
    friend var operator%(var iw, var v)  {return var(iw.load()%v.load());}
    friend var operator%(var iw, T v)    {return var(iw.load()%v);}
    friend var operator%(T v, var iw)    {return var(v%iw.load());}
    friend var operator&(var iw, var v)  {return var(iw.load()&v.load());}
    friend var operator&(var iw, T v)    {return var(iw.load()&v);}
    friend var operator&(T v, var iw)    {return var(v&iw.load());}
    friend var operator|(var iw, var v)  {return var(iw.load()|v.load());}
    friend var operator|(var iw, T v)    {return var(iw.load()|v);}
    friend var operator|(T v, var iw)    {return var(v|iw.load());}
    friend var operator^(var iw, var v)  {return var(iw.load()^v.load());}
    friend var operator^(var iw, T v)    {return var(iw.load()^v);}
    friend var operator^(T v, var iw)    {return var(v^iw.load());}
    friend var operator<<(var iw, var v) {return var(iw.load()<<v.load());}
    friend var operator<<(var iw, T v)   {return var(iw.load()<<v);}
    friend var operator<<(T v, var iw)   {return var(v<<iw.load());}
    friend var operator>>(var iw, var v) {return var(iw.load()>>v.load());}
    friend var operator>>(var iw, T v)   {return var(iw.load()>>v);}
    friend var operator>>(T v, var iw)   {return var(v>>iw.load());}

private:

}; // var

} //namespace librace


#endif //#define INCLUDED_librace2_hpp_e8be4557_fdea_42b4_8df5_a1660e69d955
