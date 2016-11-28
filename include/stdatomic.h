/**
 * @file stdatomic.h
 * @brief C11 atomic interface header
 */

#ifndef __STDATOMIC_H__
#define __STDATOMIC_H__

#include "impatomic.h"

#ifdef __cplusplus


using std::atomic_flag;


using std::atomic_bool;


using std::atomic_address;


using std::atomic_char;


using std::atomic_schar;


using std::atomic_uchar;


using std::atomic_short;


using std::atomic_ushort;


using std::atomic_int;


using std::atomic_uint;


using std::atomic_long;


using std::atomic_ulong;


using std::atomic_llong;


using std::atomic_ullong;


using std::atomic_wchar_t;


/****** More atomic types *****/
using std::atomic_int_least8_t;
using std::atomic_uint_least8_t;
using std::atomic_int_least16_t;
using std::atomic_uint_least16_t;
using std::atomic_int_least32_t;
using std::atomic_uint_least32_t;
using std::atomic_int_least64_t;
using std::atomic_uint_least64_t;

using std::atomic_int_fast8_t;
using std::atomic_uint_fast8_t;
using std::atomic_int_fast16_t;
using std::atomic_uint_fast16_t;
using std::atomic_int_fast32_t;
using std::atomic_uint_fast32_t;
using std::atomic_int_fast64_t;
using std::atomic_uint_fast64_t;

using std::atomic_intptr_t;
using std::atomic_uintptr_t;

using std::atomic_ssize_t;
using std::atomic_size_t;

using std::atomic_ptrdiff_t;

using std::atomic_intmax_t;
using std::atomic_uintmax_t;



using std::atomic;
using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

using std::atomic_thread_fence;
using std::atomic_signal_fence;

#endif /* __cplusplus */

#endif /* __STDATOMIC_H__ */
