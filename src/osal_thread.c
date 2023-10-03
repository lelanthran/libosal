
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>

#ifdef PLATFORM_Windows
#include <process.h>
#endif


#include "osal_thread.h"

#ifdef PLATFORM_Windows
typedef unsigned int thread_return_t;
#else
typedef void *thread_return_t;
#endif

struct trunner_param_t {
   osal_thread_func_t *fptr;
   void *param;
};

static thread_return_t trunner (void *param)
{
   struct trunner_param_t *tr = param;
   tr->fptr (tr->param);
   free (tr);
#ifdef PLATFORM_Windows
   return 0;
#else
   return NULL;
#endif
}

/* ***************************************************** */

#ifdef PLATFORM_Windows

bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr, void *param)
{
   struct trunner_param_t *tr = malloc (sizeof *tr);
   if (!tr) {
      return false;
   }

   tr->fptr = fptr;
   tr->param = param;

   *thandle = (HANDLE)_beginthreadex (NULL, 0, trunner, tr, 0, NULL);

   return *thandle != 0;
}

bool osal_thread_wait (osal_thread_t *threads, size_t nthreads)
{
   bool rc = true;
   for (size_t i=0; i<nthreads; i++) {
      DWORD e = WaitForSingleObject (threads[i], INFINITE);
      rc = rc && e == WAIT_OBJECT_0;
   }
   return rc;
}

void osal_thread_sleep (size_t milliseconds)
{
   DWORD mask = (DWORD)0xffffffffULL;
   DWORD param = (DWORD)(micro_s & mask);
   Sleep (param);
}

void osal_thread_del (osal_thread_t *thandle)
{
    CloseHandle (*thandle);
}

bool osal_mutex_new (osal_mutex_t *mutex)
{
   *mutex = CreateMutex (NULL, false, NULL);
   return *mutex != NULL;
}

void osal_mutex_del (osal_mutex_t *mutex)
{
   CloseHandle (*mutex);
}

bool osal_mutex_acquire (osal_mutex_t *mutex)
{
   DWORD rc = WaitForSingleObject (*mutex, 1);
   return rc == WAIT_OBJECT_0;
}

void osal_mutex_release (osal_mutex_t *mutex)
{
   ReleaseMutex (*mutex);
}

#endif

/* ***************************************************** */
#ifdef PLATFORM_POSIX


bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr, void *param)
{
   struct trunner_param_t *tr = malloc (sizeof *tr);
   if (!tr) {
      return false;
   }

   tr->fptr = fptr;
   tr->param = param;

   return pthread_create(thandle, NULL, trunner, tr) == 0;
}

bool osal_thread_wait (osal_thread_t *threads, size_t nthreads)
{
   bool ret = true;
   for (size_t i=0; i< nthreads; i++) {
      if (threads[i] == (uint64_t)-1) {
         continue;
      }

      int rc = pthread_join (threads[i], NULL);
      if (rc == 0) {
         threads[i] = (uint64_t)-1;
      }

      ret = ret && rc==0;
   }
   return ret;
}

void osal_thread_sleep (size_t micro_s)
{
   struct timespec tv, rem;

   tv.tv_sec = micro_s / 1000;
   tv.tv_nsec = (micro_s % 1000) * 1000000;

   nanosleep (&tv, &rem);
}

void osal_thread_del (osal_thread_t *thandle)
{
    (void)thandle;
}

bool osal_mutex_new (osal_mutex_t *mutex)
{
   return pthread_mutex_init(mutex, NULL) == 0;
}

void osal_mutex_del (osal_mutex_t *mutex)
{
   pthread_mutex_destroy (mutex);
}

bool osal_mutex_acquire (osal_mutex_t *mutex)
{
   for (size_t i=0; i<5; i++) {
      if ((pthread_mutex_trylock (mutex)) == 0) {
         return true;
      }
   }
   return false;
}

bool osal_mutex_release (osal_mutex_t *mutex)
{
   for (size_t i=0; i<5; i++) {
      if ((pthread_mutex_unlock (mutex)) == 0) {
         return true;
      }
   }
   return false;
}

uint64_t osal_atomic_load (volatile uint64_t *dst)
{
   // For Windows: InterlockedExchangeAnd64 (dst, 0xffffffffffffffff);
   uint64_t ret;
   __atomic_load (dst, &ret, __ATOMIC_ACQUIRE);
   return ret;
}

void osal_atomic_store (volatile uint64_t *dst, uint64_t value)
{
   // For Windows: InterlockedExchangeAdd64 (dst, 0);
   __atomic_store (dst, &value, __ATOMIC_RELEASE);
}


bool osal_cmpxchange (volatile uint64_t *target,
                      uint64_t newval, uint64_t comparand)
{
   // For Windows: InterlockedCompareExchange64 (dst, newval, comparand);
   return __atomic_compare_exchange_n (target,
                                       &comparand,
                                       newval,
                                       false,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
}

#endif


bool osal_ftex_acquire (uint64_t *target, const char *id)
{
   (void)id;
   for (size_t i=0; i<5; i++) {
      if (osal_cmpxchange (target, 1, 0)) {
         return true;
      }
   }
   return false;
}

bool osal_ftex_release (uint64_t *target, const char *id)
{
   (void)id;
   for (size_t i=0; i<5; i++) {
      if (osal_cmpxchange (target, 0, 1)) {
         return true;
      }
   }
   return false;
}

