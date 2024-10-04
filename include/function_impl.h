/*
 *  $Id: $
 *
 *  Copyright 2024 Aurelian Melinte.
 *  Released under GPL 3.0 or later.
 *
 *  You need a C++0x compiler.
 *
 */

#ifndef INCLUDED_funtion_impl_hpp_1df17659_ccc9_4662_9e2a_3253adb4d603
#define INCLUDED_funtion_impl_hpp_1df17659_ccc9_4662_9e2a_3253adb4d603

#pragma once

#include <cassert>
#include <iostream>
#include <utility>
#include <tuple>
#include <type_traits>

namespace impl {

//==============================================================================
// Poor man's function & task (cdschecker has limited std support 
// and std::function is messing with the memory addresses being checked)
//==============================================================================

// Type checkers: std::is_same<ret_t, void>::value
template<typename T>
struct is_void
{
    static const bool value = false;
};

template<>
struct is_void<void>
{
    static const bool value = true;
};


// callable interface
template<typename RET, typename... ARGS>
struct callable
{
    using ret_t       = RET ;
    using signature_t = ret_t (*)(ARGS...);

    virtual ~callable() {}

    virtual ret_t operator()(ARGS... args) = 0;   
};


// function & lambda closure wrapper
template<typename CLOSURE, typename RET, typename...ARGS>
struct closure: public callable<RET, ARGS...>
{
    using ret_t     = RET;
    using closure_t = CLOSURE;

    const closure_t _closureHandler;

    closure(const CLOSURE& handler)
        : _closureHandler(handler)
    {}

    ret_t operator()(ARGS... args) override
    {
        if constexpr (is_void<ret_t>::value) {
            (void)_closureHandler(std::forward<ARGS>(args)...);
        } else {
            return _closureHandler(std::forward<ARGS>(args)...);
        }
    }
};


// function<> template catch-all
template <typename FUNCTION>
class function
    : public function<decltype(&FUNCTION::operator())>
{
};


// function, lambda, functor...
template <typename RET, typename... ARGS>
class function<RET(*)(ARGS...)>
{

public:

    using ret_t         = RET;
    using signature_t   = ret_t (*)(ARGS...);
    using function_t    = function<signature_t> ;
    
    callable<ret_t, ARGS...>* _callableClosure;

    function(RET(*function)(ARGS...))
        : _callableClosure(new closure<signature_t, ret_t, ARGS...>(function))
    {}

    // Captured lambda specialization
    template<typename CLOSURE>
    function(const CLOSURE& func)
        : _callableClosure(new closure<decltype(func), ret_t, ARGS...>(func))
    {}
    
    ~function()
    {
        delete _callableClosure;
    }

    ret_t operator()(ARGS... args)
    {
        if constexpr (is_void<ret_t>::value) {
            (void)(*_callableClosure)(std::forward<ARGS>(args)...);
        } else {
            return (*_callableClosure)(std::forward<ARGS>(args)...);
        }
    }
};

// Member method
template <typename CLASS, typename RET, typename... ARGS>
class function<RET(CLASS::*)(ARGS...)>
{

public:

    using ret_t         = RET;
    using signature_t   = ret_t (CLASS::*)(ARGS...);
    using function_t    = function<signature_t> ;

    signature_t _methodSignature;

    function(RET(CLASS::*method)(ARGS...))
        : _methodSignature(method)
    {}

    ret_t operator()(CLASS* object, ARGS... args)
    {
        if constexpr (is_void<ret_t>::value) {
            (void)(object->*_methodSignature)(std::forward<ARGS>(args)...);
        } else {
            return (object->*_methodSignature)(std::forward<ARGS>(args)...);
        }
    }
};

// Const member method
template <typename CLASS, typename RET, typename... ARGS>
class function<RET(CLASS::*)(ARGS...) const>
{

public:

    using ret_t         = RET;
    using signature_t   = ret_t (CLASS::*)(ARGS...) const;
    using function_t    = function<signature_t> ;

    signature_t _methodSignature;

    function(RET(CLASS::*method)(ARGS...) const)
        : _methodSignature(method)
    {}

    ret_t operator()(CLASS* object, ARGS... args)
    {
        if constexpr (is_void<ret_t>::value) {
            (void)(object->*_methodSignature)(std::forward<ARGS>(args)...);
        } else {
            return (object->*_methodSignature)(std::forward<ARGS>(args)...);
        }
    }
};

} //namespace impl


#endif //#define INCLUDED_funtion_impl_hpp_1df17659_ccc9_4662_9e2a_3253adb4d603
