
#ifdef _MSC_VER

#include <intrin.h>

#include "msvc_stdatomic.h"

#ifndef _WIN64
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

__int64 _InterlockedCompareExchange64(__int64 volatile* Destination, __int64 Exchange, __int64 Comperand);
#pragma intrinsic(_InterlockedCompareExchange64)

#ifdef InterlockedIncrement64
#define _InterlockedIncrement64    InterlockedIncrement64 
#endif
#ifdef InterlockedDecrement64
#define _InterlockedDecrement64    InterlockedDecrement64
#endif
#ifdef InterlockedExchangeAdd64
#define _InterlockedExchangeAdd64  InterlockedExchangeAdd64
#endif
#ifdef InterlockedOr64
#define _InterlockedOr64           InterlockedOr64
#endif
#ifdef InterlockedAnd64
#define _InterlockedAnd64          InterlockedAnd64
#endif
#ifdef InterlockedXor64
#define _InterlockedXor64          InterlockedXor64
#endif
#ifdef InterlockedExchange64
#define _InterlockedExchange64     InterlockedExchange64
#endif
#endif


int8_t zenny_atomic_fetch_add8(volatile atomic_schar* object, int8_t operand)
{
    int8_t expected = *object;
    int8_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    } while ((success = zenny_atomic_compare_exchange8(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int16_t zenny_atomic_fetch_add16(volatile atomic_short* object, int16_t operand)
{
    int16_t expected = *object;
    int16_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    } while ((success = zenny_atomic_compare_exchange16(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int32_t zenny_atomic_fetch_add32(volatile atomic_long* object, int32_t operand)
{
    int32_t expected = *object;
    int32_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    }
    while ((success = zenny_atomic_compare_exchange32(object, &expected, desired)),
        (success? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int64_t zenny_atomic_fetch_add64(volatile atomic_llong* object, int64_t operand)
{
    int64_t expected = *object;
    int64_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    } while ((success = zenny_atomic_compare_exchange64(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int8_t zenny_atomic_fetch_sub8(volatile atomic_schar* object, int8_t operand)
{
    int8_t expected = *object;
    int8_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success = zenny_atomic_compare_exchange8(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int16_t zenny_atomic_fetch_sub16(volatile atomic_short* object, int16_t operand)
{
    int16_t expected = *object;
    int16_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success = zenny_atomic_compare_exchange16(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int32_t zenny_atomic_fetch_sub32(volatile atomic_long* object, int32_t operand)
{
    int32_t expected = *object;
    int32_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success = zenny_atomic_compare_exchange32(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int64_t zenny_atomic_fetch_sub64(volatile atomic_llong* object, int64_t operand)
{
    int64_t expected = *object;
    int64_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success = zenny_atomic_compare_exchange64(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

int8_t zenny_atomic_fetch_or8(volatile atomic_schar* object, int8_t operand)
{
    return _InterlockedOr8(object, operand);
}

int16_t zenny_atomic_fetch_or16(volatile atomic_short* object, int16_t operand)
{
    return _InterlockedOr16(object, operand);
}

int32_t zenny_atomic_fetch_or32(volatile atomic_long* object, int32_t operand)
{
    return _InterlockedOr(object, operand);
}

int64_t zenny_atomic_fetch_or64(volatile atomic_llong* object, int64_t operand)
{
    return _InterlockedOr64(object, operand);
}

int8_t zenny_atomic_fetch_xor8(volatile atomic_schar* object, int8_t operand)
{
    return _InterlockedXor8(object, operand);
}

int16_t zenny_atomic_fetch_xor16(volatile atomic_short* object, int16_t operand)
{
    return _InterlockedXor16(object, operand);
}

int32_t zenny_atomic_fetch_xor32(volatile atomic_long* object, int32_t operand)
{
    return _InterlockedXor(object, operand);
}

int64_t zenny_atomic_fetch_xor64(volatile atomic_llong* object, int64_t operand)
{
    return _InterlockedXor64(object, operand);
}

int8_t zenny_atomic_fetch_and8(volatile atomic_schar* object, int8_t operand)
{
    return _InterlockedAnd8(object, operand);
}

int16_t zenny_atomic_fetch_and16(volatile atomic_short* object, int16_t operand)
{
    return _InterlockedAnd16(object, operand);
}

int32_t zenny_atomic_fetch_and32(volatile atomic_long* object, int32_t operand)
{
    return _InterlockedAnd(object, operand);
}

int64_t zenny_atomic_fetch_and64(volatile atomic_llong* object, int64_t operand)
{
    return _InterlockedAnd64(object, operand);
}

int8_t zenny_atomic_exchange8(volatile atomic_schar* object, int8_t desired)
{
    return _InterlockedExchange8(object, desired);
}

int16_t zenny_atomic_exchange16(volatile atomic_short* object, int16_t desired)
{
    return _InterlockedExchange16(object, desired);
}

int32_t zenny_atomic_exchange32(volatile atomic_long* object, int32_t desired)
{
    return _InterlockedExchange(object, desired);
}

int64_t zenny_atomic_exchange64(volatile atomic_llong* object, int64_t desired)
{
    return _InterlockedExchange64(object, desired);
}

bool zenny_atomic_compare_exchange8(volatile atomic_schar* object, int8_t* expected, int8_t desired)
{
    int8_t comparand = *expected;
    const int8_t dstValue = _InterlockedCompareExchange8(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        *expected = dstValue;

    return success;
}

bool zenny_atomic_compare_exchange16(volatile atomic_short* object, int16_t* expected, int16_t desired)
{
    int16_t comparand = *expected;
    const int16_t dstValue = _InterlockedCompareExchange16(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        * expected = dstValue;

    return success;
}

bool zenny_atomic_compare_exchange32(volatile atomic_long* object, int32_t* expected, int32_t desired)
{
    int32_t comparand = *expected;
    int32_t dstValue = _InterlockedCompareExchange(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        *expected = dstValue;

    return success;
}

bool zenny_atomic_compare_exchange64(volatile atomic_llong* object, int64_t* expected, int64_t desired)
{
    int64_t comparand = *expected;
    int64_t dstValue = _InterlockedCompareExchange64(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        *expected = dstValue;

    return success;
}

bool atomic_flag_test_and_set(volatile atomic_flag *flag)
{
    return _InterlockedExchange8((volatile char*)flag, 1) == 1;
}

bool atomic_flag_test_and_set_explicit(volatile atomic_flag* object, memory_order order)
{
    return atomic_flag_test_and_set(object);
}

void atomic_flag_clear(volatile atomic_flag *flag)
{
    _InterlockedExchange8((volatile char*)flag, 0);
}

void atomic_flag_clear_explicit(volatile atomic_flag* object, memory_order order)
{
    atomic_flag_clear(object);
}

#endif
