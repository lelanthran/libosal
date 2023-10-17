/* ********************************************************
 * Copyright ©2023 Lelanthran Manickum, All rights reserved
 * This project  is licensed under the GPLv3.  See the file
 * LICENSE for more information.
 */

#if 1
#ifdef PLATFORM_POSIX
#define _GNU_SOURCE 200809L
#endif
#endif


#ifdef PLATFORM_Windows
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


#include <time.h>

#include "osal_timer.h"

#ifdef OSTYPE_Darwin // Provide our own implementation
#define CLOCK_REALTIME    0x2d4e1588
#define CLOCK_MONOTONIC   0x0
#endif

#ifdef OSTYPE_Darwin // Provide our own implementation
   int clock_gettime(int clock_id, struct timespec *ts);
#endif

struct osal_timer_t {
   uint64_t tte_nsecs;     // Time to expiry in nanoseconds
};



/* TODO: Insert the changes required for the WIN32 API as well.
 * 1.    bool QueryPerformanceFrequency (LARGE_INTEGER *out)
 *    This function returns a 64-bit integer (see below). This
 *    return is the number of ticks incremented per second. It
 *    is fixed at system boot (and can therefore be cached the
 *    first time it is used) => store in TICKS_PER_SEC.
 *
 * 2.    bool QueryPerformanceCounter (LARGE_INTEGER *out)
 *    This function returns the number of ticks of the performance
 *    counter. Divide by TICKS_PER_SEC to get the counter in
 *    seconds.
 *
 * 3.    GetTickCount64()
 *    This function returns the number of milliseconds since the
 *    system was booted. Potentially a lot easier to use than the
 *    above functions but not as accurate (can be 5ms - 8ms off).
 *    Might be good enough for this library.
 *
 * 4. Use a private static variable to implement UIDs for the timers
 *    (Win32, at the very least, needs this. On POSIX this may be
 *    needed as well, depending on how I implement the callbacks).
 *
typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  };
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
 */

static const uint64_t num_ns_in_sec = 1000000000ULL;
static uint64_t start_counter = 0;

/* get_time_now() must be rewritten for each target platform. It must
 * return a timestamp in nanoseconds. This function must return -1
 * on error, so ensure that the return value is never -1 on success.
 */
#ifdef PLATFORM_POSIX

#ifdef OSTYPE_Linux
#define CLOCK_ID           CLOCK_MONOTONIC_COARSE
#endif

#ifdef OSTYPE_FreeBSD
#define CLOCK_ID           CLOCK_MONOTONIC_FAST
#endif

#ifdef OSTYPE_Darwin
#define CLOCK_ID          CLOCK_REALTIME
#endif

#ifndef CLOCK_ID
#define CLOCK_ID CLOCK_MONOTONIC_COARSE
#endif

static int g_clock_id = CLOCK_ID;


#ifdef OSTYPE_Darwin
/*
 * Below we provide an alternative for clock_gettime,
 * which is not implemented in Mac OS X.
 *
 */
#include <errno.h>

int clock_gettime(int clock_id, struct timespec *ts)
{
    struct timeval tv;

    if (clock_id != CLOCK_REALTIME)
    {
        errno = EINVAL;
        return -1;
    }
    if (gettimeofday(&tv, NULL) < 0)
    {
        return -1;
    }
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
    return 0;
}
#endif



static uint64_t get_time_now (void)
{
   struct timespec rt;
   uint64_t now;

   if (clock_gettime (g_clock_id, &rt)!=0)
      return (uint64_t)-1;

   now = rt.tv_sec * num_ns_in_sec;
   now += rt.tv_nsec;

   if (now==(uint64_t)-1) {
      now++;
   }
   return now;
}
#endif



#ifdef PLATFORM_Windows
static uint64_t ticks_per_sec = 0;   // For windows, see function #1 above.
static uint64_t get_time_now (void)
{
   LARGE_INTEGER large_int_type;
   uint64_t retval;
   if (!ticks_per_sec) {
      if (!QueryPerformanceFrequency (&large_int_type)) {
         return (uint64_t)-1;
      }
      ticks_per_sec = large_int_type.QuadPart;
   }
   if (!QueryPerformanceCounter (&large_int_type)) {
      return (uint64_t)-1;
   }

   // TODO: Fix this on Windows.
   retval = large_int_type.QuadPart / (ticks_per_sec / num_ms_in_sec);

   if (retval==(uint64_t)-1) {
      retval++;
   }
   return retval;
}
#endif


uint64_t osal_timer_resolution (void)
{
   struct timespec ts = { -1, -1 };
   clock_getres (g_clock_id, &ts);
   return ts.tv_nsec;
}

bool osal_timer_setclock (enum osal_clock_t clk)
{
   // TODO: try to figure out how this will be ifdef'ed for Windows and Darwin.
   switch (clk) {
      case osal_clock_COARSE: g_clock_id = CLOCK_MONOTONIC_COARSE;   return true;
      case osal_clock_PRECISE: g_clock_id = CLOCK_MONOTONIC;         return true;
      case osal_clock_RAW: g_clock_id = CLOCK_MONOTONIC_RAW;         return true;
   }

   return false;
}


void osal_timer_init (void)
{
   osal_timer_mark_ns ();
   start_counter = get_time_now ();
}

osal_timer_t *osal_timer_set (uint64_t nanos)
{
   osal_timer_t *ret = malloc (sizeof *ret);
   if (!ret)
      return NULL;
   if (!(osal_timer_reset (ret, nanos))) {
      free (ret);
      return NULL;
   }
   return ret;
}

bool osal_timer_reset (osal_timer_t *xt, uint64_t nanos)
{
   uint64_t now = get_time_now ();
   if (now == (uint64_t)-1)
      return false;

   if (!xt)
      return false;

   memset (xt, 0, sizeof *xt);

   xt->tte_nsecs = now + nanos;

   return true;
}

bool osal_timer_expired (osal_timer_t *xt)
{
   uint64_t now = get_time_now ();

   if (now==(uint64_t)-1)
      return true;

   if (!xt)
      return true;

   if (now >= xt->tte_nsecs)
      return true;
   else
      return false;
}

uint64_t osal_timer_since_start (void)
{
   uint64_t now = get_time_now ();
   if (now==(uint64_t)-1)
      return (uint64_t)-1;

   return now - start_counter;
}

uint64_t osal_timer_mark_ns (void)
{
   static uint64_t time_us_prev;
   uint64_t time_us_now;
   uint64_t retval;

   time_us_now = get_time_now ();
   if (time_us_now == (uint64_t)-1)
      return -1;

   if (!time_us_prev) {
      time_us_prev = time_us_now;
      return 0;
   }

   retval = time_us_now - time_us_prev;

   time_us_prev = time_us_now;

   return retval;
}

void osal_timer_del (osal_timer_t *xt)
{
   if (!xt)
      return;

   free (xt);
}

